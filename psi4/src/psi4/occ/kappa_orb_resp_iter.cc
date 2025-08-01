/*
 * @BEGIN LICENSE
 *
 * Psi4: an open-source quantum chemistry software package
 *
 * Copyright (c) 2007-2025 The Psi4 Developers.
 *
 * The copyrights for code used from other parties are included in
 * the corresponding files.
 *
 * This file is part of Psi4.
 *
 * Psi4 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * Psi4 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License along
 * with Psi4; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * @END LICENSE
 */

#include "psi4/libtrans/integraltransform.h"
#include "psi4/libmints/matrix.h"
#include "psi4/libpsio/psio.hpp"
#include "occwave.h"
#include "defines.h"

#include <cmath>

namespace psi {
namespace occwave {

void OCCWave::kappa_orb_resp_iter() {
    // outfile->Printf("\n kappa_orb_resp_iter is starting... \n");

    if (reference_ == "RESTRICTED") {
        // Build M inverse and kappa
        for (int x = 0; x < nidpA; x++) {
            int a = idprowA[x];
            int i = idpcolA[x];
            int h = idpirrA[x];
            double value = FockA->get(h, a + occpiA[h], a + occpiA[h]) - FockA->get(h, i, i);
            kappaA->set(x, -wogA->get(x) / (2.0 * value));
            Minv_pcgA->set(x, 0.5 / value);
        }

        // Open dpd files
        psio_->open(PSIF_LIBTRANS_DPD, PSIO_OPEN_OLD);
        psio_->open(PSIF_OCC_DPD, PSIO_OPEN_OLD);
        dpdbuf4 K;
        dpdfile2 P, F, S;

        // Sort some integrals
        // (OV|OV) -> (VO|VO)
        global_dpd_->buf4_init(&K, PSIF_LIBTRANS_DPD, 0, ID("[O,V]"), ID("[O,V]"), ID("[O,V]"), ID("[O,V]"), 0,
                               "MO Ints (OV|OV)");
        global_dpd_->buf4_sort(&K, PSIF_LIBTRANS_DPD, qpsr, ID("[V,O]"), ID("[V,O]"), "MO Ints (VO|VO)");
        global_dpd_->buf4_close(&K);

        // <OO|VV> -> <OV|VO>
        global_dpd_->buf4_init(&K, PSIF_LIBTRANS_DPD, 0, ID("[O,O]"), ID("[V,V]"), ID("[O,O]"), ID("[V,V]"), 0,
                               "MO Ints <OO|VV>");
        global_dpd_->buf4_sort(&K, PSIF_LIBTRANS_DPD, psrq, ID("[O,V]"), ID("[V,O]"), "MO Ints <OV|VO>");
        global_dpd_->buf4_close(&K);

        // <OV|OV> -> <VO|VO>
        global_dpd_->buf4_init(&K, PSIF_LIBTRANS_DPD, 0, ID("[O,V]"), ID("[O,V]"), ID("[O,V]"), ID("[O,V]"), 0,
                               "MO Ints <OV|OV>");
        global_dpd_->buf4_sort(&K, PSIF_LIBTRANS_DPD, qpsr, ID("[V,O]"), ID("[V,O]"), "MO Ints <VO|VO>");
        global_dpd_->buf4_close(&K);

        // Build Sigma_0 = A * k0
        // Write p vector to dpdfile2
        global_dpd_->file2_init(&P, PSIF_OCC_DPD, 0, ID('V'), ID('O'), "P <V|O>");
        global_dpd_->file2_mat_init(&P);
        int idp_idx = 0;
        for (int h = 0; h < nirrep_; ++h) {
            for (int a = 0; a < virtpiA[h]; ++a) {
                for (int i = 0; i < occpiA[h]; ++i) {
                    P.matrix[h][a][i] = kappaA->get(idp_idx);
                    idp_idx++;
                }
            }
        }
        global_dpd_->file2_mat_wrt(&P);
        global_dpd_->file2_close(&P);

        // We are computing Ax, but storing x as d.
        compute_sigma_vector();

        // Build r0
        r_pcgA->zero();
        r_pcgA->subtract(wogA);
        r_pcgA->scale(lambda_damping);  // new
        r_pcgA->subtract(sigma_pcgA);

        // Construct initial S
        S_pcgA->dirprd(Minv_pcgA, r_pcgA);

        // Construct initial D
        D_pcgA->copy(S_pcgA);

        // Call Orbital Response Solver
        orb_resp_pcg_rhf();

        // Close dpd files
        psio_->close(PSIF_OCC_DPD, 1);
        psio_->close(PSIF_LIBTRANS_DPD, 1);

        // If PCG FAILED!
        if (pcg_conver == 0) {
            // Build kappa again
            for (int x = 0; x < nidpA; x++) {
                int a = idprowA[x];
                int i = idpcolA[x];
                int h = idpirrA[x];
                double value = FockA->get(h, a + occpiA[h], a + occpiA[h]) - FockA->get(h, i, i);
                kappaA->set(x, -wogA->get(x) / (2.0 * value));
            }

            if (print_ > 1) {
                outfile->Printf("\tWarning!!! PCG did NOT converged in %2d iterations, switching to MSD. \n", itr_pcg);
            }
        }  // end if pcg_conver = 0

        // find biggest_kappa
        biggest_kappaA = 0;
        for (int i = 0; i < nidpA; i++) {
            if (std::fabs(kappaA->get(i)) > biggest_kappaA) biggest_kappaA = std::fabs(kappaA->get(i));
        }

        // Scale
        if (biggest_kappaA > step_max) {
            for (int i = 0; i < nidpA; i++) kappaA->set(i, kappaA->get(i) * (step_max / biggest_kappaA));
        }

        // find biggest_kappa again
        if (biggest_kappaA > step_max) {
            biggest_kappaA = 0;
            for (int i = 0; i < nidpA; i++) {
                if (std::fabs(kappaA->get(i)) > biggest_kappaA) {
                    biggest_kappaA = std::fabs(kappaA->get(i));
                }
            }
        }

        // norm
        rms_kappaA = 0;
        rms_kappaA = kappaA->rms();

        // print
        if (print_ > 2) kappaA->print();

    }  // end if (reference_ == "RESTRICTED")

    else if (reference_ == "UNRESTRICTED") {
        // Build M inverse and kappa
        // alpha
        for (int x = 0; x < nidpA; x++) {
            int a = idprowA[x];
            int i = idpcolA[x];
            int h = idpirrA[x];
            double value = FockA->get(h, a + occpiA[h], a + occpiA[h]) - FockA->get(h, i, i);
            kappaA->set(x, -wogA->get(x) / (2.0 * value));
            Minv_pcgA->set(x, 0.5 / value);
        }

        // beta
        for (int x = 0; x < nidpB; x++) {
            int a = idprowB[x];
            int i = idpcolB[x];
            int h = idpirrB[x];
            double value = FockB->get(h, a + occpiB[h], a + occpiB[h]) - FockB->get(h, i, i);
            kappaB->set(x, -wogB->get(x) / (2.0 * value));
            Minv_pcgB->set(x, 0.5 / value);
        }

        // Open dpd files
        psio_->open(PSIF_LIBTRANS_DPD, PSIO_OPEN_OLD);
        psio_->open(PSIF_OCC_DPD, PSIO_OPEN_OLD);
        dpdbuf4 K;
        dpdfile2 P, F, S;

        // Sort some integrals
        // (OV|OV) -> (VO|VO)
        global_dpd_->buf4_init(&K, PSIF_LIBTRANS_DPD, 0, ID("[O,V]"), ID("[O,V]"), ID("[O,V]"), ID("[O,V]"), 0,
                               "MO Ints (OV|OV)");
        global_dpd_->buf4_sort(&K, PSIF_LIBTRANS_DPD, qpsr, ID("[V,O]"), ID("[V,O]"), "MO Ints (VO|VO)");
        global_dpd_->buf4_close(&K);

        // (ov|ov) -> (vo|vo)
        global_dpd_->buf4_init(&K, PSIF_LIBTRANS_DPD, 0, ID("[o,v]"), ID("[o,v]"), ID("[o,v]"), ID("[o,v]"), 0,
                               "MO Ints (ov|ov)");
        global_dpd_->buf4_sort(&K, PSIF_LIBTRANS_DPD, qpsr, ID("[v,o]"), ID("[v,o]"), "MO Ints (vo|vo)");
        global_dpd_->buf4_close(&K);

        // <OO|VV> -> <OV|VO>
        global_dpd_->buf4_init(&K, PSIF_LIBTRANS_DPD, 0, ID("[O,O]"), ID("[V,V]"), ID("[O,O]"), ID("[V,V]"), 0,
                               "MO Ints <OO|VV>");
        global_dpd_->buf4_sort(&K, PSIF_LIBTRANS_DPD, psrq, ID("[O,V]"), ID("[V,O]"), "MO Ints <OV|VO>");
        global_dpd_->buf4_close(&K);

        // <oo|vv> -> <ov|vo>
        global_dpd_->buf4_init(&K, PSIF_LIBTRANS_DPD, 0, ID("[o,o]"), ID("[v,v]"), ID("[o,o]"), ID("[v,v]"), 0,
                               "MO Ints <oo|vv>");
        global_dpd_->buf4_sort(&K, PSIF_LIBTRANS_DPD, psrq, ID("[o,v]"), ID("[v,o]"), "MO Ints <ov|vo>");
        global_dpd_->buf4_close(&K);

        // <OV|OV> -> <VO|VO>
        global_dpd_->buf4_init(&K, PSIF_LIBTRANS_DPD, 0, ID("[O,V]"), ID("[O,V]"), ID("[O,V]"), ID("[O,V]"), 0,
                               "MO Ints <OV|OV>");
        global_dpd_->buf4_sort(&K, PSIF_LIBTRANS_DPD, qpsr, ID("[V,O]"), ID("[V,O]"), "MO Ints <VO|VO>");
        global_dpd_->buf4_close(&K);

        // <ov|ov> -> <vo|vo>
        global_dpd_->buf4_init(&K, PSIF_LIBTRANS_DPD, 0, ID("[o,v]"), ID("[o,v]"), ID("[o,v]"), ID("[o,v]"), 0,
                               "MO Ints <ov|ov>");
        global_dpd_->buf4_sort(&K, PSIF_LIBTRANS_DPD, qpsr, ID("[v,o]"), ID("[v,o]"), "MO Ints <vo|vo>");
        global_dpd_->buf4_close(&K);

        // (OV|ov) -> (VO|vo)
        global_dpd_->buf4_init(&K, PSIF_LIBTRANS_DPD, 0, ID("[O,V]"), ID("[o,v]"), ID("[O,V]"), ID("[o,v]"), 0,
                               "MO Ints (OV|ov)");
        global_dpd_->buf4_sort(&K, PSIF_LIBTRANS_DPD, qpsr, ID("[V,O]"), ID("[v,o]"), "MO Ints (VO|vo)");
        global_dpd_->buf4_close(&K);

        // (VO|vo) -> (vo|VO)
        global_dpd_->buf4_init(&K, PSIF_LIBTRANS_DPD, 0, ID("[V,O]"), ID("[v,o]"), ID("[V,O]"), ID("[v,o]"), 0,
                               "MO Ints (VO|vo)");
        global_dpd_->buf4_sort(&K, PSIF_LIBTRANS_DPD, rspq, ID("[v,o]"), ID("[V,O]"), "MO Ints (vo|VO)");
        global_dpd_->buf4_close(&K);

        // Build Sigma_0 = A * k0
        // Write alpha p vector to dpdfile2
        global_dpd_->file2_init(&P, PSIF_OCC_DPD, 0, ID('V'), ID('O'), "P <V|O>");
        global_dpd_->file2_mat_init(&P);
        idp_idx = 0;
        for (int h = 0; h < nirrep_; ++h) {
            for (int a = 0; a < virtpiA[h]; ++a) {
                for (int i = 0; i < occpiA[h]; ++i) {
                    P.matrix[h][a][i] = kappaA->get(idp_idx);
                    idp_idx++;
                }
            }
        }
        global_dpd_->file2_mat_wrt(&P);
        global_dpd_->file2_close(&P);

        // Write beta p vector to dpdfile2
        global_dpd_->file2_init(&P, PSIF_OCC_DPD, 0, ID('v'), ID('o'), "P <v|o>");
        global_dpd_->file2_mat_init(&P);
        idp_idx = 0;
        for (int h = 0; h < nirrep_; ++h) {
            for (int a = 0; a < virtpiB[h]; ++a) {
                for (int i = 0; i < occpiB[h]; ++i) {
                    P.matrix[h][a][i] = kappaB->get(idp_idx);
                    idp_idx++;
                }
            }
        }
        global_dpd_->file2_mat_wrt(&P);
        global_dpd_->file2_close(&P);

        // We are computing Ax, but storing x as d.
        compute_sigma_vector();

        // Build r0
        r_pcgA->zero();
        r_pcgA->subtract(wogA);
        r_pcgA->scale(lambda_damping);  // new
        r_pcgA->subtract(sigma_pcgA);
        r_pcgB->zero();
        r_pcgB->subtract(wogB);
        r_pcgB->scale(lambda_damping);  // new
        r_pcgB->subtract(sigma_pcgB);

        // Construct initial S
        S_pcgA->dirprd(Minv_pcgA, r_pcgA);
        S_pcgB->dirprd(Minv_pcgB, r_pcgB);

        // Construct initial D
        D_pcgA->copy(S_pcgA);
        D_pcgB->copy(S_pcgB);

        // Call Orbital Response Solver
        orb_resp_pcg_uhf();
        // outfile->Printf(" rms_pcg: %12.10f\n", rms_pcg);
        //

        // Close dpd files
        psio_->close(PSIF_OCC_DPD, 1);
        psio_->close(PSIF_LIBTRANS_DPD, 1);

        // If PCG FAILED!
        if (pcg_conver == 0) {
            // Build kappa again
            // alpha
            for (int x = 0; x < nidpA; x++) {
                int a = idprowA[x];
                int i = idpcolA[x];
                int h = idpirrA[x];
                double value = FockA->get(h, a + occpiA[h], a + occpiA[h]) - FockA->get(h, i, i);
                kappaA->set(x, -wogA->get(x) / (2.0 * value));
            }

            // beta
            for (int x = 0; x < nidpB; x++) {
                int a = idprowB[x];
                int i = idpcolB[x];
                int h = idpirrB[x];
                double value = FockB->get(h, a + occpiB[h], a + occpiB[h]) - FockB->get(h, i, i);
                kappaB->set(x, -wogB->get(x) / (2.0 * value));
            }

            if (print_ > 1) {
                outfile->Printf("\tWarning!!! PCG did NOT converged in %2d iterations, switching to MSD. \n", itr_pcg);
            }
        }  // en d if pcg_conver = 0

        // find biggest_kappa
        biggest_kappaA = 0;
        for (int i = 0; i < nidpA; i++) {
            if (std::fabs(kappaA->get(i)) > biggest_kappaA) biggest_kappaA = std::fabs(kappaA->get(i));
        }

        biggest_kappaB = 0;
        for (int i = 0; i < nidpB; i++) {
            if (std::fabs(kappaB->get(i)) > biggest_kappaB) biggest_kappaB = std::fabs(kappaB->get(i));
        }

        // Scale
        if (biggest_kappaA > step_max) {
            for (int i = 0; i < nidpA; i++) kappaA->set(i, kappaA->get(i) * (step_max / biggest_kappaA));
        }

        if (biggest_kappaB > step_max) {
            for (int i = 0; i < nidpB; i++) kappaB->set(i, kappaB->get(i) * (step_max / biggest_kappaB));
        }

        // find biggest_kappa again
        if (biggest_kappaA > step_max) {
            biggest_kappaA = 0;
            for (int i = 0; i < nidpA; i++) {
                if (std::fabs(kappaA->get(i)) > biggest_kappaA) {
                    biggest_kappaA = std::fabs(kappaA->get(i));
                }
            }
        }

        if (biggest_kappaB > step_max) {
            biggest_kappaB = 0;
            for (int i = 0; i < nidpB; i++) {
                if (std::fabs(kappaB->get(i)) > biggest_kappaB) {
                    biggest_kappaB = std::fabs(kappaB->get(i));
                }
            }
        }

        // norm
        rms_kappaA = 0;
        rms_kappaB = 0;
        rms_kappaA = kappaA->rms();
        rms_kappaB = kappaB->rms();

        // print
        if (print_ > 2) {
            kappaA->print();
            kappaB->print();
        }

    }  // end if (reference_ == "UNRESTRICTED")
       // outfile->Printf("\n kappa_orb_resp_iter done. \n");
}  // end kappa_orb_resp_iter

void OCCWave::orb_resp_pcg_rhf() {
    itr_pcg = 0;
    idp_idx = 0;
    double rms_r_pcgA = 0.0;
    double beta;
    pcg_conver = 1;  // assuming pcg will converge

    double delta_new = r_pcgA->dot(S_pcgA);

    // Head of the loop
    do {
        dpdfile2 P;

        // Write p vector to dpdfile2
        global_dpd_->file2_init(&P, PSIF_OCC_DPD, 0, ID('V'), ID('O'), "P <V|O>");
        global_dpd_->file2_mat_init(&P);
        idp_idx = 0;
        for (int h = 0; h < nirrep_; ++h) {
            for (int a = 0; a < virtpiA[h]; ++a) {
                for (int i = 0; i < occpiA[h]; ++i) {
                    P.matrix[h][a][i] = D_pcgA->get(idp_idx);
                    idp_idx++;
                }
            }
        }
        global_dpd_->file2_mat_wrt(&P);
        global_dpd_->file2_close(&P);

        compute_sigma_vector();
        
        // Build line search parameter alpha
        double alpha = delta_new / D_pcgA->dot(sigma_pcgA);

        // Build kappa-new
        kappa_newA->zero();
        kappa_newA->copy(D_pcgA);
        kappa_newA->scale(alpha);
        kappa_newA->add(kappaA);

        // Build r-new
        r_pcg_newA->zero();
        r_pcg_newA->copy(sigma_pcgA);
        r_pcg_newA->scale(-alpha);
        r_pcg_newA->add(r_pcgA);
        rms_r_pcgA = r_pcg_newA->rms();

        // Update S
        S_pcgA->dirprd(Minv_pcgA, r_pcg_newA);

        double delta_old = delta_new;
        delta_new = r_pcg_newA->dot(S_pcgA);

        // Build line search parameter beta
        if (pcg_beta_type_ == "FLETCHER_REEVES") {
            beta = delta_new / delta_old;
        }

        else if (pcg_beta_type_ == "POLAK_RIBIERE") {
            dr_pcgA->copy(r_pcg_newA);
            dr_pcgA->subtract(r_pcgA);
            beta = S_pcgA->dot(dr_pcgA) / delta_old;
        }

        // Update D
        D_pcgA->scale(beta);
        D_pcgA->add(S_pcgA);

        // Reset
        kappaA->zero();
        r_pcgA->zero();
        kappaA->copy(kappa_newA);
        r_pcgA->copy(r_pcg_newA);

        // RMS kappa
        rms_kappaA = 0.0;
        rms_kappaA = kappaA->rms();
        rms_pcg = rms_kappaA;

        // increment iteration index
        itr_pcg++;

        // If we exceed maximum number of iteration, break the loop
        if (itr_pcg >= pcg_maxiter) {
            pcg_conver = 0;
            break;
        }

        if (rms_r_pcgA < tol_pcg || rms_kappaA < tol_pcg) break;

    } while (std::fabs(rms_pcg) >= tol_pcg);

}  // end orb_resp_pcg_rhf

void OCCWave::orb_resp_pcg_uhf() {
    itr_pcg = 0;
    idp_idx = 0;
    double rms_r_pcgA = 0.0;
    double rms_r_pcgB = 0.0;
    double rms_r_pcg = 0.0;
    double beta;
    pcg_conver = 1;  // assuming pcg will converge

    double delta_new = r_pcgA->dot(S_pcgA) + r_pcgB->dot(S_pcgB);

    // Head of the loop
    do {
        dpdfile2 P;

        // Write alpha p vector to dpdfile2
        global_dpd_->file2_init(&P, PSIF_OCC_DPD, 0, ID('V'), ID('O'), "P <V|O>");
        global_dpd_->file2_mat_init(&P);
        idp_idx = 0;
        for (int h = 0; h < nirrep_; ++h) {
            for (int a = 0; a < virtpiA[h]; ++a) {
                for (int i = 0; i < occpiA[h]; ++i) {
                    P.matrix[h][a][i] = D_pcgA->get(idp_idx);
                    idp_idx++;
                }
            }
        }
        global_dpd_->file2_mat_wrt(&P);
        global_dpd_->file2_close(&P);

        // Write beta p vector to dpdfile2
        global_dpd_->file2_init(&P, PSIF_OCC_DPD, 0, ID('v'), ID('o'), "P <v|o>");
        global_dpd_->file2_mat_init(&P);
        idp_idx = 0;
        for (int h = 0; h < nirrep_; ++h) {
            for (int a = 0; a < virtpiB[h]; ++a) {
                for (int i = 0; i < occpiB[h]; ++i) {
                    P.matrix[h][a][i] = D_pcgB->get(idp_idx);
                    idp_idx++;
                }
            }
        }
        global_dpd_->file2_mat_wrt(&P);
        global_dpd_->file2_close(&P);

        compute_sigma_vector();

        // Build line search parameter alpha
        double alpha = delta_new / (D_pcgA->dot(sigma_pcgA) + D_pcgB->dot(sigma_pcgB));

        // Build kappa-new
        kappa_newA->zero();
        kappa_newA->copy(D_pcgA);
        kappa_newA->scale(alpha);
        kappa_newA->add(kappaA);
        kappa_newB->zero();
        kappa_newB->copy(D_pcgB);
        kappa_newB->scale(alpha);
        kappa_newB->add(kappaB);

        // Build r-new
        r_pcg_newA->zero();
        r_pcg_newA->copy(sigma_pcgA);
        r_pcg_newA->scale(-alpha);
        r_pcg_newA->add(r_pcgA);
        rms_r_pcgA = r_pcg_newA->rms();
        r_pcg_newB->zero();
        r_pcg_newB->copy(sigma_pcgB);
        r_pcg_newB->scale(-alpha);
        r_pcg_newB->add(r_pcgB);
        rms_r_pcgB = r_pcg_newB->rms();
        rms_r_pcg = MAX0(rms_r_pcgA, rms_r_pcgB);

        // Update S
        S_pcgA->dirprd(Minv_pcgA, r_pcg_newA);
        S_pcgB->dirprd(Minv_pcgB, r_pcg_newB);

        double delta_old = delta_new;
        delta_new = r_pcg_newA->dot(S_pcgA) + r_pcg_newB->dot(S_pcgB);

        // Build line search parameter beta
        if (pcg_beta_type_ == "FLETCHER_REEVES") {
            beta = delta_new / delta_old;
        }

        else if (pcg_beta_type_ == "POLAK_RIBIERE") {
            dr_pcgA->copy(r_pcg_newA);
            dr_pcgA->subtract(r_pcgA);
            dr_pcgB->copy(r_pcg_newB);
            dr_pcgB->subtract(r_pcgB);
            beta = (S_pcgA->dot(dr_pcgA) + S_pcgB->dot(dr_pcgB)) / delta_old;
        }

        // Update D
        D_pcgA->scale(beta);
        D_pcgA->add(S_pcgA);
        D_pcgB->scale(beta);
        D_pcgB->add(S_pcgB);

        // Reset
        kappaA->zero();
        r_pcgA->zero();
        kappaA->copy(kappa_newA);
        r_pcgA->copy(r_pcg_newA);
        kappaB->zero();
        r_pcgB->zero();
        kappaB->copy(kappa_newB);
        r_pcgB->copy(r_pcg_newB);

        // RMS kappa
        rms_kappaA = kappaA->rms();
        rms_kappaB = kappaB->rms();
        rms_kappa = MAX0(rms_kappaA, rms_kappaB);
        rms_pcg = rms_kappa;

        // increment iteration index
        itr_pcg++;

        // If we exceed maximum number of iteration, break the loop
        if (itr_pcg >= pcg_maxiter) {
            pcg_conver = 0;
            break;
        }

        if (rms_r_pcg < tol_pcg || rms_kappa < tol_pcg) break;

    } while (std::fabs(rms_pcg) >= tol_pcg);

}  // end orb_resp_pcg_uhf

// Computes the product of the HF MO hessian and the conjugate direction vector, P.
// P is assumed to be stored in the D PCG dpdfile. Result is written to S.
void OCCWave::compute_sigma_vector() {
    dpdbuf4 K;
    dpdfile2 S, P, F;

    if (reference_ == "RESTRICTED") {
        // Build sigma = A * p

        // sigma_AI = 2 \sum_{B} F_AB P_BJ
        global_dpd_->file2_init(&S, PSIF_OCC_DPD, 0, ID('V'), ID('O'), "Sigma <V|O>");
        global_dpd_->file2_init(&P, PSIF_OCC_DPD, 0, ID('V'), ID('O'), "P <V|O>");
        // CAUTION! "FD" libdpd entries are the Fock elements. "F" is off-diagonal only.
        // We need all Fock elements, so use "FD", and not "F".
        global_dpd_->file2_init(&F, PSIF_LIBTRANS_DPD, 0, ID('V'), ID('V'), "FD <V|V>");
        global_dpd_->contract222(&F, &P, &S, 0, 1, 2.0, 0.0);
        global_dpd_->file2_close(&F);

        // sigma_AI -= 2 \sum_{J} P_AJ F_JI
        global_dpd_->file2_init(&F, PSIF_LIBTRANS_DPD, 0, ID('O'), ID('O'), "FD <O|O>");
        global_dpd_->contract222(&P, &F, &S, 0, 1, -2.0, 1.0);
        global_dpd_->file2_close(&F);

        // sigma_ai += 8 \sum_{bj} (ai|bj) P_bj
        global_dpd_->buf4_init(&K, PSIF_LIBTRANS_DPD, 0, ID("[V,O]"), ID("[V,O]"), ID("[V,O]"), ID("[V,O]"), 0,
                               "MO Ints (VO|VO)");
        global_dpd_->contract422(&K, &P, &S, 0, 0, 8.0, 1.0);
        global_dpd_->buf4_close(&K);

        // sigma_ai -= 2 \sum_{bj} <ia|bj> P_bj
        global_dpd_->buf4_init(&K, PSIF_LIBTRANS_DPD, 0, ID("[O,V]"), ID("[V,O]"), ID("[O,V]"), ID("[V,O]"), 0,
                               "MO Ints <OV|VO>");
        global_dpd_->contract422(&K, &P, &S, 0, 1, -2.0, 1.0);
        global_dpd_->buf4_close(&K);

        // sigma_ai -= 2 \sum_{bj} <ai|bj> P_bj
        global_dpd_->buf4_init(&K, PSIF_LIBTRANS_DPD, 0, ID("[V,O]"), ID("[V,O]"), ID("[V,O]"), ID("[V,O]"), 0,
                               "MO Ints <VO|VO>");
        global_dpd_->contract422(&K, &P, &S, 0, 0, -2.0, 1.0);
        global_dpd_->buf4_close(&K);
        global_dpd_->file2_close(&P);
        global_dpd_->file2_close(&S);

        // Read sigma vector from dpdfile2
        global_dpd_->file2_init(&S, PSIF_OCC_DPD, 0, ID('V'), ID('O'), "Sigma <V|O>");
        global_dpd_->file2_mat_init(&S);
        global_dpd_->file2_mat_rd(&S);
        idp_idx = 0;
        for (int h = 0; h < nirrep_; ++h) {
            for (int a = 0; a < virtpiA[h]; ++a) {
                for (int i = 0; i < occpiA[h]; ++i) {
                    sigma_pcgA->set(idp_idx, S.matrix[h][a][i]);
                    idp_idx++;
                }
            }
        }
        global_dpd_->file2_close(&P);
        global_dpd_->file2_close(&S);

    } else if (reference_ == "UNRESTRICTED") {
        // Start to alpha spin case
        // Build sigma = A * p

        // sigma_AI = 2 \sum_{B} F_AB P_BI
        global_dpd_->file2_init(&S, PSIF_OCC_DPD, 0, ID('V'), ID('O'), "Sigma <V|O>");
        global_dpd_->file2_init(&P, PSIF_OCC_DPD, 0, ID('V'), ID('O'), "P <V|O>");
        // CAUTION! "FD" libdpd entries are the Fock elements. "F" is off-diagonal only.
        // We need all Fock elements, so use "FD", and not "F".
        global_dpd_->file2_init(&F, PSIF_LIBTRANS_DPD, 0, ID('V'), ID('V'), "FD <V|V>");
        global_dpd_->contract222(&F, &P, &S, 0, 1, 2.0, 0.0);
        global_dpd_->file2_close(&F);

        // sigma_AI -= 2 \sum_{J} P_AJ F_JI
        global_dpd_->file2_init(&F, PSIF_LIBTRANS_DPD, 0, ID('O'), ID('O'), "FD <O|O>");
        global_dpd_->contract222(&P, &F, &S, 0, 1, -2.0, 1.0);
        global_dpd_->file2_close(&F);

        // sigma_AI += 4 \sum_{BJ} (AI|BJ) P_BJ
        global_dpd_->buf4_init(&K, PSIF_LIBTRANS_DPD, 0, ID("[V,O]"), ID("[V,O]"), ID("[V,O]"), ID("[V,O]"), 0,
                               "MO Ints (VO|VO)");
        global_dpd_->contract422(&K, &P, &S, 0, 0, 4.0, 1.0);
        global_dpd_->buf4_close(&K);

        // sigma_AI -= 2 \sum_{BJ} <IA|BJ> P_BJ
        global_dpd_->buf4_init(&K, PSIF_LIBTRANS_DPD, 0, ID("[O,V]"), ID("[V,O]"), ID("[O,V]"), ID("[V,O]"), 0,
                               "MO Ints <OV|VO>");
        global_dpd_->contract422(&K, &P, &S, 0, 1, -2.0, 1.0);
        global_dpd_->buf4_close(&K);

        // sigma_AI -= 2 \sum_{BJ} <AI|BJ> P_BJ
        global_dpd_->buf4_init(&K, PSIF_LIBTRANS_DPD, 0, ID("[V,O]"), ID("[V,O]"), ID("[V,O]"), ID("[V,O]"), 0,
                               "MO Ints <VO|VO>");
        global_dpd_->contract422(&K, &P, &S, 0, 0, -2.0, 1.0);
        global_dpd_->buf4_close(&K);
        global_dpd_->file2_close(&P);

        // sigma_AI += 4 \sum_{bj} (AI|bj) P_bj
        global_dpd_->file2_init(&P, PSIF_OCC_DPD, 0, ID('v'), ID('o'), "P <v|o>");
        global_dpd_->buf4_init(&K, PSIF_LIBTRANS_DPD, 0, ID("[V,O]"), ID("[v,o]"), ID("[V,O]"), ID("[v,o]"), 0,
                               "MO Ints (VO|vo)");
        global_dpd_->contract422(&K, &P, &S, 0, 0, 4.0, 1.0);
        global_dpd_->buf4_close(&K);
        global_dpd_->file2_close(&P);
        global_dpd_->file2_close(&S);

        // Read sigma vector from dpdfile2
        global_dpd_->file2_init(&S, PSIF_OCC_DPD, 0, ID('V'), ID('O'), "Sigma <V|O>");
        global_dpd_->file2_mat_init(&S);
        global_dpd_->file2_mat_rd(&S);
        idp_idx = 0;
        for (int h = 0; h < nirrep_; ++h) {
            for (int a = 0; a < virtpiA[h]; ++a) {
                for (int i = 0; i < occpiA[h]; ++i) {
                    sigma_pcgA->set(idp_idx, S.matrix[h][a][i]);
                    idp_idx++;
                }
            }
        }
        global_dpd_->file2_close(&S);

        // Start to beta spin case
        // Build sigma = A * p

        // sigma_ai = 2 \sum_{b} F_ab P_bi
        global_dpd_->file2_init(&S, PSIF_OCC_DPD, 0, ID('v'), ID('o'), "Sigma <v|o>");
        global_dpd_->file2_init(&P, PSIF_OCC_DPD, 0, ID('v'), ID('o'), "P <v|o>");
        global_dpd_->file2_init(&F, PSIF_LIBTRANS_DPD, 0, ID('v'), ID('v'), "FD <v|v>");
        global_dpd_->contract222(&F, &P, &S, 0, 1, 2.0, 0.0);
        global_dpd_->file2_close(&F);

        // sigma_ai -= 2 \sum_{j} P_aj F_ji
        global_dpd_->file2_init(&F, PSIF_LIBTRANS_DPD, 0, ID('o'), ID('o'), "FD <o|o>");
        global_dpd_->contract222(&P, &F, &S, 0, 1, -2.0, 1.0);
        global_dpd_->file2_close(&F);

        // sigma_ai += 4 \sum_{bj} (ai|bj) P_bj
        global_dpd_->buf4_init(&K, PSIF_LIBTRANS_DPD, 0, ID("[v,o]"), ID("[v,o]"), ID("[v,o]"), ID("[v,o]"), 0,
                               "MO Ints (vo|vo)");
        global_dpd_->contract422(&K, &P, &S, 0, 0, 4.0, 1.0);
        global_dpd_->buf4_close(&K);

        // sigma_ai -= 2 \sum_{bj} <ia|bj> P_bj
        global_dpd_->buf4_init(&K, PSIF_LIBTRANS_DPD, 0, ID("[o,v]"), ID("[v,o]"), ID("[o,v]"), ID("[v,o]"), 0,
                               "MO Ints <ov|vo>");
        global_dpd_->contract422(&K, &P, &S, 0, 1, -2.0, 1.0);
        global_dpd_->buf4_close(&K);

        // sigma_ai -= 2 \sum_{bj} <ai|bj> P_bj
        global_dpd_->buf4_init(&K, PSIF_LIBTRANS_DPD, 0, ID("[v,o]"), ID("[v,o]"), ID("[v,o]"), ID("[v,o]"), 0,
                               "MO Ints <vo|vo>");
        global_dpd_->contract422(&K, &P, &S, 0, 0, -2.0, 1.0);
        global_dpd_->buf4_close(&K);
        global_dpd_->file2_close(&P);

        // sigma_ai += 4 \sum_{BJ} (ai|BJ) P_BJ
        global_dpd_->file2_init(&P, PSIF_OCC_DPD, 0, ID('V'), ID('O'), "P <V|O>");
        global_dpd_->buf4_init(&K, PSIF_LIBTRANS_DPD, 0, ID("[v,o]"), ID("[V,O]"), ID("[v,o]"), ID("[V,O]"), 0,
                               "MO Ints (vo|VO)");
        global_dpd_->contract422(&K, &P, &S, 0, 0, 4.0, 1.0);
        global_dpd_->buf4_close(&K);
        global_dpd_->file2_close(&P);
        global_dpd_->file2_close(&S);

        // Read sigma vector from dpdfile2
        global_dpd_->file2_init(&S, PSIF_OCC_DPD, 0, ID('v'), ID('o'), "Sigma <v|o>");
        global_dpd_->file2_mat_init(&S);
        global_dpd_->file2_mat_rd(&S);
        idp_idx = 0;
        for (int h = 0; h < nirrep_; ++h) {
            for (int a = 0; a < virtpiB[h]; ++a) {
                for (int i = 0; i < occpiB[h]; ++i) {
                    sigma_pcgB->set(idp_idx, S.matrix[h][a][i]);
                    idp_idx++;
                }
            }
        }
        global_dpd_->file2_close(&S);
    }
}

}
}  // End Namespaces

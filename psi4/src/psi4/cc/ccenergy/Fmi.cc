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

/*! \file
    \ingroup CCENERGY
    \brief Enter brief description of file here
*/
#include <cstdio>
#include <cstdlib>
#include "psi4/libdpd/dpd.h"
#include "Params.h"
#include "psi4/cc/ccwave.h"
#include "psi4/libmints/matrix.h"

namespace psi {
namespace ccenergy {

void CCEnergyWavefunction::Fmi_build() {
    dpdfile2 FMI, Fmi, FMIt, Fmit, fIJ, fij, fIA, fia;
    dpdfile2 tIA, tia, FME, Fme;
    dpdbuf4 E_anti, E, D_anti, D;
    dpdbuf4 tautIJAB, tautijab, tautIjAb;

    if (params_.ref == 0) { /** RHF **/
        global_dpd_->file2_init(&fIJ, PSIF_CC_OEI, 0, 0, 0, "fIJ");
        global_dpd_->file2_copy(&fIJ, PSIF_CC_OEI, "FMI");
        global_dpd_->file2_close(&fIJ);
    } else if (params_.ref == 1) { /** ROHF **/
        global_dpd_->file2_init(&fIJ, PSIF_CC_OEI, 0, 0, 0, "fIJ");
        global_dpd_->file2_copy(&fIJ, PSIF_CC_OEI, "FMI");
        global_dpd_->file2_close(&fIJ);

        global_dpd_->file2_init(&fij, PSIF_CC_OEI, 0, 0, 0, "fij");
        global_dpd_->file2_copy(&fij, PSIF_CC_OEI, "Fmi");
        global_dpd_->file2_close(&fij);
    } else if (params_.ref == 2) { /** UHF **/
        global_dpd_->file2_init(&fIJ, PSIF_CC_OEI, 0, 0, 0, "fIJ");
        global_dpd_->file2_copy(&fIJ, PSIF_CC_OEI, "FMI");
        global_dpd_->file2_close(&fIJ);

        global_dpd_->file2_init(&fij, PSIF_CC_OEI, 0, 2, 2, "fij");
        global_dpd_->file2_copy(&fij, PSIF_CC_OEI, "Fmi");
        global_dpd_->file2_close(&fij);
    }

    // For RHF, we don't zero.
    // This is because the amplitude update in Psi's RHF uses the full residual while Psi's ROHF and UHF updates separate
    // out the diagonal contributions from the Fock matrix [cf. Eqs. (1) and (2) of Stanton et al., J. Chem. Phys. 94, 4334-4345 (1991)].
    if (params_.ref == 1 || params_.ref == 2) { /** ROHF | UHF **/
        int param = params_.ref == 1 ? 0 : 2; // ROHF needs 0, UHF needs 2
        global_dpd_->file2_init(&FMI, PSIF_CC_OEI, 0, 0, 0, "FMI");
        Matrix temp(&FMI);
        temp.zero_diagonal();
        temp.write_to_dpdfile2(&FMI);
        global_dpd_->file2_close(&FMI);

        global_dpd_->file2_init(&Fmi, PSIF_CC_OEI, 0, param, param, "Fmi");
        temp = Matrix(&Fmi);
        temp.zero_diagonal();
        temp.write_to_dpdfile2(&Fmi);
        global_dpd_->file2_close(&Fmi);
    }

    if (params_.ref == 0) { /** RHF **/
        global_dpd_->file2_init(&FMI, PSIF_CC_OEI, 0, 0, 0, "FMI");

        global_dpd_->file2_init(&fIA, PSIF_CC_OEI, 0, 0, 1, "fIA");
        global_dpd_->file2_init(&tIA, PSIF_CC_OEI, 0, 0, 1, "tIA");
        global_dpd_->contract222(&fIA, &tIA, &FMI, 0, 0, 0.5, 1);
        global_dpd_->file2_close(&tIA);
        global_dpd_->file2_close(&fIA);

        global_dpd_->buf4_init(&E_anti, PSIF_CC_EINTS, 0, 11, 0, 11, 0, 1, "E <ai|jk>");
        global_dpd_->buf4_init(&E, PSIF_CC_EINTS, 0, 11, 0, 11, 0, 0, "E <ai|jk>");

        global_dpd_->file2_init(&tIA, PSIF_CC_OEI, 0, 0, 1, "tIA");

        global_dpd_->dot13(&tIA, &E_anti, &FMI, 1, 1, 1.0, 1.0);
        global_dpd_->dot13(&tIA, &E, &FMI, 1, 1, 1.0, 1.0);

        global_dpd_->file2_close(&tIA);

        global_dpd_->buf4_close(&E_anti);
        global_dpd_->buf4_close(&E);

        global_dpd_->buf4_init(&D, PSIF_CC_DINTS, 0, 0, 5, 0, 5, 0, "D 2<ij|ab> - <ij|ba>");
        global_dpd_->buf4_init(&tautIjAb, PSIF_CC_TAMPS, 0, 0, 5, 0, 5, 0, "tautIjAb");
        global_dpd_->contract442(&D, &tautIjAb, &FMI, 0, 0, 1, 1);
        global_dpd_->buf4_close(&tautIjAb);
        global_dpd_->buf4_close(&D);

        /* Build the tilde intermediate */
        global_dpd_->file2_copy(&FMI, PSIF_CC_OEI, "FMIt");
        global_dpd_->file2_close(&FMI);

        global_dpd_->file2_init(&FMIt, PSIF_CC_OEI, 0, 0, 0, "FMIt");

        global_dpd_->file2_init(&tIA, PSIF_CC_OEI, 0, 0, 1, "tIA");
        global_dpd_->file2_init(&FME, PSIF_CC_OEI, 0, 0, 1, "FME");
        global_dpd_->contract222(&FME, &tIA, &FMIt, 0, 0, 0.5, 1);
        global_dpd_->file2_close(&FME);
        global_dpd_->file2_close(&tIA);

        global_dpd_->file2_close(&FMIt);
    } else if (params_.ref == 1) { /** ROHF **/

        global_dpd_->file2_init(&FMI, PSIF_CC_OEI, 0, 0, 0, "FMI");
        global_dpd_->file2_init(&Fmi, PSIF_CC_OEI, 0, 0, 0, "Fmi");

        global_dpd_->file2_init(&fIA, PSIF_CC_OEI, 0, 0, 1, "fIA");
        global_dpd_->file2_init(&tIA, PSIF_CC_OEI, 0, 0, 1, "tIA");
        global_dpd_->contract222(&fIA, &tIA, &FMI, 0, 0, 0.5, 1);
        global_dpd_->file2_close(&tIA);
        global_dpd_->file2_close(&fIA);

        global_dpd_->file2_init(&fia, PSIF_CC_OEI, 0, 0, 1, "fia");
        global_dpd_->file2_init(&tia, PSIF_CC_OEI, 0, 0, 1, "tia");
        global_dpd_->contract222(&fia, &tia, &Fmi, 0, 0, 0.5, 1);
        global_dpd_->file2_close(&tia);
        global_dpd_->file2_close(&fia);

        global_dpd_->buf4_init(&E_anti, PSIF_CC_EINTS, 0, 11, 0, 11, 0, 1, "E <ai|jk>");
        global_dpd_->buf4_init(&E, PSIF_CC_EINTS, 0, 11, 0, 11, 0, 0, "E <ai|jk>");
        global_dpd_->file2_init(&tIA, PSIF_CC_OEI, 0, 0, 1, "tIA");
        global_dpd_->file2_init(&tia, PSIF_CC_OEI, 0, 0, 1, "tia");

        global_dpd_->dot13(&tIA, &E_anti, &FMI, 1, 1, 1.0, 1.0);
        global_dpd_->dot13(&tia, &E, &FMI, 1, 1, 1.0, 1.0);

        global_dpd_->dot13(&tia, &E_anti, &Fmi, 1, 1, 1.0, 1.0);
        global_dpd_->dot13(&tIA, &E, &Fmi, 1, 1, 1.0, 1.0);

        global_dpd_->file2_close(&tIA);
        global_dpd_->file2_close(&tia);
        global_dpd_->buf4_close(&E_anti);
        global_dpd_->buf4_close(&E);

        global_dpd_->buf4_init(&D_anti, PSIF_CC_DINTS, 0, 0, 7, 0, 7, 0, "D <ij||ab> (ij,a>b)");
        global_dpd_->buf4_init(&tautIJAB, PSIF_CC_TAMPS, 0, 0, 7, 2, 7, 0, "tautIJAB");
        global_dpd_->buf4_init(&tautijab, PSIF_CC_TAMPS, 0, 0, 7, 2, 7, 0, "tautijab");

        global_dpd_->contract442(&D_anti, &tautIJAB, &FMI, 0, 0, 1, 1);
        global_dpd_->contract442(&D_anti, &tautijab, &Fmi, 0, 0, 1, 1);

        global_dpd_->buf4_close(&tautIJAB);
        global_dpd_->buf4_close(&tautijab);
        global_dpd_->buf4_close(&D_anti);

        global_dpd_->buf4_init(&D, PSIF_CC_DINTS, 0, 0, 5, 0, 5, 0, "D <ij|ab>");
        global_dpd_->buf4_init(&tautIjAb, PSIF_CC_TAMPS, 0, 0, 5, 0, 5, 0, "tautIjAb");

        global_dpd_->contract442(&D, &tautIjAb, &FMI, 0, 0, 1, 1);
        global_dpd_->contract442(&D, &tautIjAb, &Fmi, 1, 1, 1, 1);

        global_dpd_->buf4_close(&tautIjAb);
        global_dpd_->buf4_close(&D);

        /* Build the tilde intermediate */
        global_dpd_->file2_copy(&FMI, PSIF_CC_OEI, "FMIt");
        global_dpd_->file2_copy(&Fmi, PSIF_CC_OEI, "Fmit");

        global_dpd_->file2_close(&FMI);
        global_dpd_->file2_close(&Fmi);

        global_dpd_->file2_init(&FMIt, PSIF_CC_OEI, 0, 0, 0, "FMIt");
        global_dpd_->file2_init(&Fmit, PSIF_CC_OEI, 0, 0, 0, "Fmit");

        global_dpd_->file2_init(&tIA, PSIF_CC_OEI, 0, 0, 1, "tIA");
        global_dpd_->file2_init(&FME, PSIF_CC_OEI, 0, 0, 1, "FME");
        global_dpd_->contract222(&FME, &tIA, &FMIt, 0, 0, 0.5, 1);
        global_dpd_->file2_close(&FME);
        global_dpd_->file2_close(&tIA);

        global_dpd_->file2_init(&tia, PSIF_CC_OEI, 0, 0, 1, "tia");
        global_dpd_->file2_init(&Fme, PSIF_CC_OEI, 0, 0, 1, "Fme");
        global_dpd_->contract222(&Fme, &tia, &Fmit, 0, 0, 0.5, 1);
        global_dpd_->file2_close(&Fme);
        global_dpd_->file2_close(&tia);

        global_dpd_->file2_close(&FMIt);
        global_dpd_->file2_close(&Fmit);
    } else if (params_.ref == 2) { /** UHF **/

        global_dpd_->file2_init(&FMI, PSIF_CC_OEI, 0, 0, 0, "FMI");
        global_dpd_->file2_init(&Fmi, PSIF_CC_OEI, 0, 2, 2, "Fmi");

        global_dpd_->file2_init(&fIA, PSIF_CC_OEI, 0, 0, 1, "fIA");
        global_dpd_->file2_init(&tIA, PSIF_CC_OEI, 0, 0, 1, "tIA");
        global_dpd_->contract222(&fIA, &tIA, &FMI, 0, 0, 0.5, 1);
        global_dpd_->file2_close(&tIA);
        global_dpd_->file2_close(&fIA);

        global_dpd_->file2_init(&fia, PSIF_CC_OEI, 0, 2, 3, "fia");
        global_dpd_->file2_init(&tia, PSIF_CC_OEI, 0, 2, 3, "tia");
        global_dpd_->contract222(&fia, &tia, &Fmi, 0, 0, 0.5, 1);
        global_dpd_->file2_close(&tia);
        global_dpd_->file2_close(&fia);

        global_dpd_->file2_init(&tIA, PSIF_CC_OEI, 0, 0, 1, "tIA");
        global_dpd_->file2_init(&tia, PSIF_CC_OEI, 0, 2, 3, "tia");

        global_dpd_->buf4_init(&E_anti, PSIF_CC_EINTS, 0, 21, 0, 21, 0, 1, "E <AI|JK>");
        global_dpd_->buf4_init(&E, PSIF_CC_EINTS, 0, 22, 24, 22, 24, 0, "E <Ij|Ka>");

        global_dpd_->dot13(&tIA, &E_anti, &FMI, 1, 1, 1, 1);
        global_dpd_->dot24(&tia, &E, &FMI, 0, 0, 1, 1);

        global_dpd_->buf4_close(&E);
        global_dpd_->buf4_close(&E_anti);

        global_dpd_->buf4_init(&E_anti, PSIF_CC_EINTS, 0, 31, 10, 31, 10, 1, "E <ai|jk>");
        global_dpd_->buf4_init(&E, PSIF_CC_EINTS, 0, 26, 22, 26, 22, 0, "E <Ai|Jk>");

        global_dpd_->dot13(&tia, &E_anti, &Fmi, 1, 1, 1, 1);
        global_dpd_->dot13(&tIA, &E, &Fmi, 1, 1, 1, 1);

        global_dpd_->buf4_close(&E);
        global_dpd_->buf4_close(&E_anti);

        global_dpd_->file2_close(&tIA);
        global_dpd_->file2_close(&tia);

        global_dpd_->buf4_init(&D, PSIF_CC_DINTS, 0, 0, 7, 0, 7, 0, "D <IJ||AB> (IJ,A>B)");
        global_dpd_->buf4_init(&tautIJAB, PSIF_CC_TAMPS, 0, 0, 7, 2, 7, 0, "tautIJAB");
        global_dpd_->contract442(&D, &tautIJAB, &FMI, 0, 0, 1, 1);
        global_dpd_->buf4_close(&tautIJAB);
        global_dpd_->buf4_close(&D);

        global_dpd_->buf4_init(&D, PSIF_CC_DINTS, 0, 10, 17, 10, 17, 0, "D <ij||ab> (ij,a>b)");
        global_dpd_->buf4_init(&tautijab, PSIF_CC_TAMPS, 0, 10, 17, 12, 17, 0, "tautijab");
        global_dpd_->contract442(&D, &tautijab, &Fmi, 0, 0, 1, 1);
        global_dpd_->buf4_close(&tautijab);
        global_dpd_->buf4_close(&D);

        global_dpd_->buf4_init(&D, PSIF_CC_DINTS, 0, 22, 28, 22, 28, 0, "D <Ij|Ab>");
        global_dpd_->buf4_init(&tautIjAb, PSIF_CC_TAMPS, 0, 22, 28, 22, 28, 0, "tautIjAb");
        global_dpd_->contract442(&D, &tautIjAb, &FMI, 0, 0, 1, 1);
        global_dpd_->buf4_close(&tautIjAb);
        global_dpd_->buf4_close(&D);

        global_dpd_->buf4_init(&D, PSIF_CC_DINTS, 0, 23, 29, 23, 29, 0, "D <iJ|aB>");
        global_dpd_->buf4_init(&tautIjAb, PSIF_CC_TAMPS, 0, 23, 29, 23, 29, 0, "tautiJaB");
        global_dpd_->contract442(&D, &tautIjAb, &Fmi, 0, 0, 1, 1);
        global_dpd_->buf4_close(&tautIjAb);
        global_dpd_->buf4_close(&D);

        /* Build the tilde intermediate */
        global_dpd_->file2_copy(&FMI, PSIF_CC_OEI, "FMIt");
        global_dpd_->file2_copy(&Fmi, PSIF_CC_OEI, "Fmit");

        global_dpd_->file2_close(&FMI);
        global_dpd_->file2_close(&Fmi);

        global_dpd_->file2_init(&FMIt, PSIF_CC_OEI, 0, 0, 0, "FMIt");
        global_dpd_->file2_init(&Fmit, PSIF_CC_OEI, 0, 2, 2, "Fmit");

        global_dpd_->file2_init(&tIA, PSIF_CC_OEI, 0, 0, 1, "tIA");
        global_dpd_->file2_init(&FME, PSIF_CC_OEI, 0, 0, 1, "FME");
        global_dpd_->contract222(&FME, &tIA, &FMIt, 0, 0, 0.5, 1);
        global_dpd_->file2_close(&FME);
        global_dpd_->file2_close(&tIA);

        global_dpd_->file2_init(&tia, PSIF_CC_OEI, 0, 2, 3, "tia");
        global_dpd_->file2_init(&Fme, PSIF_CC_OEI, 0, 2, 3, "Fme");
        global_dpd_->contract222(&Fme, &tia, &Fmit, 0, 0, 0.5, 1);
        global_dpd_->file2_close(&Fme);
        global_dpd_->file2_close(&tia);

        global_dpd_->file2_close(&FMIt);
        global_dpd_->file2_close(&Fmit);
    }
}
}  // namespace ccenergy
}  // namespace psi

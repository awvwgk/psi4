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

/***************************************************************************
 *  PSIMRCC : Copyright (C) 2007 by Francesco Evangelista and Andrew Simmonett
 *  frank@ccc.uga.edu   andysim@ccc.uga.edu
 *  A multireference coupled cluster code
 ***************************************************************************/
#include "psi4/libmoinfo/libmoinfo.h"
#include "psi4/libpsi4util/libpsi4util.h"

#include "blas.h"
#include "index.h"
#include "matrix.h"
#include "mrcc.h"

namespace psi {
namespace psimrcc {

void CCMRCC::build_t1_amplitudes() {
    build_t1_ia_amplitudes();
    build_t1_IA_amplitudes();
}

void CCMRCC::build_t1_ia_amplitudes() {
    // Closed-shell
    wfn_->blas()->append("t1_eqns[o][v]{c} = fock[o][v]{c}");
    wfn_->blas()->append("t1_eqns[o][v]{c} += t1[o][v]{c} 2@2 F_ae[v][v]{c}");
    wfn_->blas()->append("t1_eqns[o][v]{c} += - F_mi[o][o]{c} 1@1 t1[o][v]{c}");
    wfn_->blas()->append("t1_eqns[o][v]{c} += #12# t2[ov][ov]{c} 2@1 F_me[ov]{c}");
    wfn_->blas()->append("t1_eqns[o][v]{c} += #12# t2[ov][OV]{c} 2@1 F_me[ov]{c}");

    wfn_->blas()->append("t1_eqns[o][v]{c} += #12# - <[ov]|[ov]> 2@1 t1[ov]{c}");
    wfn_->blas()->append("t1_eqns[o][v]{c} += #21# 2 ([ov]|[vo]) 1@1 t1[ov]{c}");

    wfn_->blas()->append("t1_eqns[o][v]{c} += 1/2 t2[o][ovv]{c} 2@2 <[v]:[ovv]>");
    wfn_->blas()->append("t1_eqns[o][v]{c} +=     t2[o][OvV]{c} 2@2 <[v]|[ovv]>");

    wfn_->blas()->append("t1_eqns[o][v]{c} += -1/2 <[o]:[voo]> 2@2 t2[v][voo]{c}");
    wfn_->blas()->append("t1_eqns[o][v]{c} += - <[o]|[voo]> 2@2 t2[v][VoO]{c}");

    // Open-shell
    wfn_->blas()->append("t1_eqns[o][v]{o} = fock[o][v]{o}");
    wfn_->blas()->append("t1_eqns[o][v]{o} += t1[o][v]{o} 2@2 F_ae[v][v]{o}");
    wfn_->blas()->append("t1_eqns[o][v]{o} += - F_mi[o][o]{o} 1@1 t1[o][v]{o}");
    wfn_->blas()->append("t1_eqns[o][v]{o} += #12# t2[ov][ov]{o} 2@1 F_me[ov]{o}");
    wfn_->blas()->append("t1_eqns[o][v]{o} += #12# t2[ov][OV]{o} 2@1 F_ME[OV]{o}");

    wfn_->blas()->append("t1_eqns[o][v]{o} += #12# - <[ov]|[ov]> 2@1 t1[ov]{o}");
    wfn_->blas()->append("t1_eqns[o][v]{o} += #21#  ([ov]|[vo]) 1@1 t1[ov]{o}");
    wfn_->blas()->append("t1_eqns[o][v]{o} += #21#  ([ov]|[vo]) 1@1 t1[OV]{o}");

    wfn_->blas()->append("t1_eqns[o][v]{o} += 1/2 t2[o][ovv]{o} 2@2 <[v]:[ovv]>");
    wfn_->blas()->append("t1_eqns[o][v]{o} +=     t2[o][OvV]{o} 2@2 <[v]|[ovv]>");

    wfn_->blas()->append("t1_eqns[o][v]{o} += -1/2 <[o]:[voo]> 2@2 t2[v][voo]{o}");
    wfn_->blas()->append("t1_eqns[o][v]{o} += - <[o]|[voo]> 2@2 t2[v][VoO]{o}");
}

void CCMRCC::build_t1_IA_amplitudes() {
    // Closed-shell
    wfn_->blas()->append("t1_eqns[O][V]{c} = t1_eqns[o][v]{c}");

    // Open-shell
    wfn_->blas()->append("t1_eqns[O][V]{o} = fock[O][V]{o}");
    wfn_->blas()->append("t1_eqns[O][V]{o} += t1[O][V]{o} 2@2 F_AE[V][V]{o}");
    wfn_->blas()->append("t1_eqns[O][V]{o} += - F_MI[O][O]{o} 1@1 t1[O][V]{o}");
    wfn_->blas()->append("t1_eqns[O][V]{o} += #12# t2[OV][OV]{o} 2@1 F_ME[OV]{o}");
    wfn_->blas()->append("t1_eqns[O][V]{o} += #12# t2[ov][OV]{o} 1@1 F_me[ov]{o}");

    wfn_->blas()->append("t1_eqns[O][V]{o} += #12# - <[ov]|[ov]> 2@1 t1[OV]{o}");
    wfn_->blas()->append("t1_eqns[O][V]{o} += #21#  ([ov]|[vo]) 1@1 t1[OV]{o}");
    wfn_->blas()->append("t1_eqns[O][V]{o} += #21#  ([ov]|[vo]) 1@1 t1[ov]{o}");

    wfn_->blas()->append("t1_eqns[O][V]{o} += 1/2 t2[O][OVV]{o} 2@2 <[v]:[ovv]>");
    wfn_->blas()->append("t1_eqns[O][V]{o} +=     t2[O][oVv]{o} 2@2 <[v]|[ovv]>");

    wfn_->blas()->append("t1_eqns[O][V]{o} += -1/2 <[o]:[voo]> 2@2 t2[V][VOO]{o}");
    wfn_->blas()->append("t1_eqns[O][V]{o} += - <[o]|[voo]> 2@2 t2[V][vOo]{o}");
}

void CCMRCC::build_t1_amplitudes_triples() {
    build_t1_ia_amplitudes_triples();
    build_t1_IA_amplitudes_triples();
}

/**
 * @brief Computes the contraction
 * \f[ \frac{1}{4} \sum_{mnef} t_{imn}^{aef} <mn||ef> \f]
 * as described in Fig. 1 of Phys. Chem. Chem. Phys. vol. 2, pg. 2047 (2000).
 * This algorithm hence performs
 * \f[ \frac{1}{4} \sum_{mnef} t_{imn}^{aef} <mn||ef> + \sum_{mNeF} t_{imN}^{aeF} <mN|eF> + \frac{1}{4} \sum_{MNEF}
 * t_{iMN}^{aEF} <MN||EF>\rightarrow  \{ \bar{H}_{i}^{a} \} \f]
 */
void CCMRCC::build_t1_ia_amplitudes_triples() {
    // Loop over references
    for (int ref = 0; ref < wfn_->moinfo()->get_nunique(); ref++) {
        int unique_ref = wfn_->moinfo()->get_ref_number(ref, UniqueRefs);

        // Grab the temporary matrices
        CCMatTmp HiaMatTmp = wfn_->blas()->get_MatTmp("t1_eqns[o][v]", unique_ref, none);
        CCMatTmp TijkabcMatTmp = wfn_->blas()->get_MatTmp("t3[ooo][vvv]", unique_ref, none);
        CCMatTmp TijKabCMatTmp = wfn_->blas()->get_MatTmp("t3[ooO][vvV]", unique_ref, none);
        CCMatTmp TiJKaBCMatTmp = wfn_->blas()->get_MatTmp("t3[oOO][vVV]", unique_ref, none);
        CCMatTmp ImnefMatTmp = wfn_->blas()->get_MatTmp("<[oo]:[vv]>", none);
        CCMatTmp ImNeFMatTmp = wfn_->blas()->get_MatTmp("<[oo]|[vv]>", none);

        // Grab the indexing for t3[ijk][abc]
        auto& mn_tuples = ImnefMatTmp->get_left()->get_tuples();
        auto& ef_tuples = ImnefMatTmp->get_right()->get_tuples();

        auto Tijkabc_matrix = TijkabcMatTmp->get_matrix();
        auto TijKabC_matrix = TijKabCMatTmp->get_matrix();
        auto TiJKaBC_matrix = TiJKaBCMatTmp->get_matrix();
        auto Hia_matrix = HiaMatTmp->get_matrix();
        auto Imnef_matrix = ImnefMatTmp->get_matrix();
        auto ImNeF_matrix = ImNeFMatTmp->get_matrix();
        CCIndex* ijkIndex = wfn_->blas()->get_index("[ooo]");
        CCIndex* abcIndex = wfn_->blas()->get_index("[vvv]");

        for (int h = 0; h < wfn_->moinfo()->get_nirreps(); h++) {
            size_t i_offset = HiaMatTmp->get_left()->get_first(h);
            size_t a_offset = HiaMatTmp->get_right()->get_first(h);
            for (int a = 0; a < HiaMatTmp->get_right_pairpi(h); a++) {
                int a_abs = a + a_offset;
                for (int i = 0; i < HiaMatTmp->get_left_pairpi(h); i++) {
                    int i_abs = i + i_offset;
                    // <mn||ef> contribution
                    for (int mn_sym = 0; mn_sym < wfn_->moinfo()->get_nirreps(); mn_sym++) {
                        size_t mn_offset = ImnefMatTmp->get_left()->get_first(mn_sym);
                        size_t ef_offset = ImnefMatTmp->get_right()->get_first(mn_sym);
                        for (int ef = 0; ef < ImnefMatTmp->get_right_pairpi(mn_sym); ef++) {
                            int e = ef_tuples[ef_offset + ef][0];
                            int f = ef_tuples[ef_offset + ef][1];
                            size_t aef = abcIndex->get_tuple_rel_index(a_abs, e, f);
                            int aef_sym = abcIndex->get_tuple_irrep(a_abs, e, f);
                            for (int mn = 0; mn < ImnefMatTmp->get_left_pairpi(mn_sym); mn++) {
                                int m = mn_tuples[mn_offset + mn][0];
                                int n = mn_tuples[mn_offset + mn][1];
                                size_t imn = ijkIndex->get_tuple_rel_index(i_abs, m, n);
                                Hia_matrix[h][i][a] +=
                                    0.25 * Tijkabc_matrix[aef_sym][imn][aef] * Imnef_matrix[mn_sym][mn][ef];
                                Hia_matrix[h][i][a] +=
                                    0.25 * TiJKaBC_matrix[aef_sym][imn][aef] * Imnef_matrix[mn_sym][mn][ef];
                                Hia_matrix[h][i][a] += TijKabC_matrix[aef_sym][imn][aef] * ImNeF_matrix[mn_sym][mn][ef];
                            }
                        }
                    }
                }
            }
        }
    }
    //   wfn_->blas()->print("t1_eqns[o][v]{u}");
    //   wfn_->blas()->solve("ERROR{u} = 100000000000.0 t1_eqns[o][v]{u} . t1_eqns[o][v]{u}");
    //   wfn_->blas()->print("ERROR{u}");
}

/**
 * @brief Computes the contraction
 * \f[ \frac{1}{4} \sum_{mnef} t_{imn}^{aef} <mn||ef> \f]
 * as described in Fig. 1 of Phys. Chem. Chem. Phys. vol. 2, pg. 2047 (2000).
 * This algorithm hence performs
 * \f[ \frac{1}{4} \sum_{mnef} t_{mnI}^{efA} <mn||ef> + \sum_{mNeF} t_{mNI}^{eFA} <mN|eF> + \frac{1}{4} \sum_{MNEF}
 * t_{MNI}^{EFA} <MN||EF>\rightarrow  \{ \bar{H}_{I}^{A} \} \f]
 */
void CCMRCC::build_t1_IA_amplitudes_triples() {
    // Loop over references
    for (int ref = 0; ref < wfn_->moinfo()->get_nunique(); ref++) {
        int unique_ref = wfn_->moinfo()->get_ref_number(ref, UniqueRefs);

        // Grab the temporary matrices
        CCMatTmp HIAMatTmp = wfn_->blas()->get_MatTmp("t1_eqns[O][V]", unique_ref, none);
        CCMatTmp TijKabCMatTmp = wfn_->blas()->get_MatTmp("t3[ooO][vvV]", unique_ref, none);
        CCMatTmp TiJKaBCMatTmp = wfn_->blas()->get_MatTmp("t3[oOO][vVV]", unique_ref, none);
        CCMatTmp TIJKABCMatTmp = wfn_->blas()->get_MatTmp("t3[OOO][VVV]", unique_ref, none);
        CCMatTmp ImnefMatTmp = wfn_->blas()->get_MatTmp("<[oo]:[vv]>", none);
        CCMatTmp ImNeFMatTmp = wfn_->blas()->get_MatTmp("<[oo]|[vv]>", none);

        // Grab the indexing for t3[ijk][abc]
        auto& mn_tuples = ImnefMatTmp->get_left()->get_tuples();
        auto& ef_tuples = ImnefMatTmp->get_right()->get_tuples();

        auto TijKabC_matrix = TijKabCMatTmp->get_matrix();
        auto TiJKaBC_matrix = TiJKaBCMatTmp->get_matrix();
        auto TIJKABC_matrix = TIJKABCMatTmp->get_matrix();
        auto HIA_matrix = HIAMatTmp->get_matrix();
        auto Imnef_matrix = ImnefMatTmp->get_matrix();
        auto ImNeF_matrix = ImNeFMatTmp->get_matrix();
        CCIndex* ijkIndex = wfn_->blas()->get_index("[ooo]");
        CCIndex* abcIndex = wfn_->blas()->get_index("[vvv]");

        for (int h = 0; h < wfn_->moinfo()->get_nirreps(); h++) {
            size_t i_offset = HIAMatTmp->get_left()->get_first(h);
            size_t a_offset = HIAMatTmp->get_right()->get_first(h);
            for (int a = 0; a < HIAMatTmp->get_right_pairpi(h); a++) {
                int a_abs = a + a_offset;
                for (int i = 0; i < HIAMatTmp->get_left_pairpi(h); i++) {
                    int i_abs = i + i_offset;
                    // <mn||ef> contribution
                    for (int mn_sym = 0; mn_sym < wfn_->moinfo()->get_nirreps(); mn_sym++) {
                        size_t mn_offset = ImnefMatTmp->get_left()->get_first(mn_sym);
                        size_t ef_offset = ImnefMatTmp->get_right()->get_first(mn_sym);
                        for (int ef = 0; ef < ImnefMatTmp->get_right_pairpi(mn_sym); ef++) {
                            int e = ef_tuples[ef_offset + ef][0];
                            int f = ef_tuples[ef_offset + ef][1];
                            size_t efa = abcIndex->get_tuple_rel_index(e, f, a_abs);
                            int efa_sym = abcIndex->get_tuple_irrep(e, f, a_abs);
                            for (int mn = 0; mn < ImnefMatTmp->get_left_pairpi(mn_sym); mn++) {
                                int m = mn_tuples[mn_offset + mn][0];
                                int n = mn_tuples[mn_offset + mn][1];
                                size_t mni = ijkIndex->get_tuple_rel_index(m, n, i_abs);
                                HIA_matrix[h][i][a] +=
                                    0.25 * TijKabC_matrix[efa_sym][mni][efa] * Imnef_matrix[mn_sym][mn][ef];
                                HIA_matrix[h][i][a] +=
                                    0.25 * TIJKABC_matrix[efa_sym][mni][efa] * Imnef_matrix[mn_sym][mn][ef];
                                HIA_matrix[h][i][a] += TiJKaBC_matrix[efa_sym][mni][efa] * ImNeF_matrix[mn_sym][mn][ef];
                            }
                        }
                    }
                }
            }
        }
    }
    //   wfn_->blas()->print("t1_eqns[O][V]{u}");
    //   wfn_->blas()->solve("ERROR{u} = 100000000000.0 t1_eqns[O][V]{u} . t1_eqns[O][V]{u}");
    //   wfn_->blas()->print("ERROR{u}");
}

}  // namespace psimrcc
}  // namespace psi

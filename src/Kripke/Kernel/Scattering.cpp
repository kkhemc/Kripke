/*
 * NOTICE
 *
 * This work was produced at the Lawrence Livermore National Laboratory (LLNL)
 * under contract no. DE-AC-52-07NA27344 (Contract 44) between the U.S.
 * Department of Energy (DOE) and Lawrence Livermore National Security, LLC
 * (LLNS) for the operation of LLNL. The rights of the Federal Government are
 * reserved under Contract 44.
 *
 * DISCLAIMER
 *
 * This work was prepared as an account of work sponsored by an agency of the
 * United States Government. Neither the United States Government nor Lawrence
 * Livermore National Security, LLC nor any of their employees, makes any
 * warranty, express or implied, or assumes any liability or responsibility
 * for the accuracy, completeness, or usefulness of any information, apparatus,
 * product, or process disclosed, or represents that its use would not infringe
 * privately-owned rights. Reference herein to any specific commercial products,
 * process, or service by trade name, trademark, manufacturer or otherwise does
 * not necessarily constitute or imply its endorsement, recommendation, or
 * favoring by the United States Government or Lawrence Livermore National
 * Security, LLC. The views and opinions of authors expressed herein do not
 * necessarily state or reflect those of the United States Government or
 * Lawrence Livermore National Security, LLC, and shall not be used for
 * advertising or product endorsement purposes.
 *
 * NOTIFICATION OF COMMERCIAL USE
 *
 * Commercialization of this product is prohibited without notifying the
 * Department of Energy (DOE) or Lawrence Livermore National Security.
 */

#include <Kripke/Kernel.h>
#include <Kripke/PartitionSpace.h>
#include <Kripke/Timing.h>
#include <Kripke/VarTypes.h>

/**
  Compute scattering source term phi_out from flux moments in phi.
  phi_out(gp,z,nm) = sum_g { sigs(g, n, gp) * phi(g,z,nm) }
*/

void Kripke::Kernel::scattering(Kripke::DataStore &data_store)
{
  KRIPKE_TIMER(data_store, Scattering);

  auto &pspace = data_store.getVariable<Kripke::PartitionSpace>("pspace");

  auto &set_group  = data_store.getVariable<Kripke::Set>("Set/Group");
  auto &set_moment = data_store.getVariable<Kripke::Set>("Set/Moment");
  auto &set_zone   = data_store.getVariable<Kripke::Set>("Set/Zone");

  auto &field_phi     = data_store.getVariable<Kripke::Field_Moments>("phi");
  auto &field_phi_out = data_store.getVariable<Kripke::Field_Moments>("phi_out");
  auto &field_sigs    = data_store.getVariable<Field_SigmaS>("data/sigs");

  auto &field_zone_to_mixelem     = data_store.getVariable<Field_Zone2MixElem>("zone_to_mixelem");
  auto &field_zone_to_num_mixelem = data_store.getVariable<Field_Zone2Int>("zone_to_num_mixelem");
  auto &field_mixed_to_material = data_store.getVariable<Field_MixElem2Material>("mixelem_to_material");
  auto &field_mixed_to_fraction = data_store.getVariable<Field_MixElem2Double>("mixelem_to_fraction");

  auto &field_moment_to_legendre = data_store.getVariable<Field_Moment2Legendre>("moment_to_legendre");

  // Zero out source term
  kConst(field_phi_out, 0.0);

  // Loop over subdomains and compute scattering source
  for(auto sdom_src : field_phi.getWorkList()){
    for(auto sdom_dst : field_phi_out.getWorkList()){

      // Only work on subdomains where src and dst are on the same R subdomain
      size_t r_src = pspace.subdomainToSpace(SPACE_R, sdom_src);
      size_t r_dst = pspace.subdomainToSpace(SPACE_R, sdom_dst);
      if(r_src != r_dst){
        continue;
      }

      // Get glower for src and dst ranges (to index into sigma_s)
      int glower_src = set_group.lower(sdom_src);
      int glower_dst = set_group.lower(sdom_dst);


      // get material mix information
      auto moment_to_legendre = field_moment_to_legendre.getView(sdom_src);

      auto phi = field_phi.getView(sdom_src);
      auto phi_out = field_phi_out.getView(sdom_dst);
      auto sigs = field_sigs.getView(sdom_src);


      auto zone_to_mixelem     = field_zone_to_mixelem.getView(sdom_src);
      auto zone_to_num_mixelem = field_zone_to_num_mixelem.getView(sdom_src);
      auto mixelem_to_material = field_mixed_to_material.getView(sdom_src);
      auto mixelem_to_fraction = field_mixed_to_fraction.getView(sdom_src);

      // grab dimensions
      int num_zones =      set_zone.size(sdom_src);
      int num_groups_src = set_group.size(sdom_src);
      int num_groups_dst = set_group.size(sdom_dst);
      int num_moments =    set_moment.size(sdom_dst);

      for(Moment nm{0};nm < num_moments;++ nm){
        for(Group g{0};g < num_groups_dst;++ g){ // dst group
          for(Group gp{0};gp < num_groups_src;++ gp){ // src group
            for(Zone z{0};z < num_zones;++ z){

              // map nm to n
              Legendre n = moment_to_legendre(nm);

              GlobalGroup global_g{*g+glower_dst};
              GlobalGroup global_gp{*gp+glower_src};

              MixElem mix_start = zone_to_mixelem(z);
              MixElem mix_stop = mix_start + zone_to_num_mixelem(z);

              for(MixElem mix = mix_start;mix < mix_stop;++ mix){
                Material mat = mixelem_to_material(mix);
                double fraction = mixelem_to_fraction(mix);

                phi_out(nm, g, z) +=
                    sigs(mat, n, global_g, global_gp)
                    * phi(nm, gp, z)
                    * fraction;
              }
            }
          }
        }
      }
    }
  }
}




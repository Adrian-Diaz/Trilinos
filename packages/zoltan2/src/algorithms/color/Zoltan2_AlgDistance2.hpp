#ifndef _ZOLTAN2_DISTANCE2_HPP_
#define _ZOLTAN2_DISTANCE2_HPP_

#include <vector>
#include <unordered_map>
#include <iostream>
#include <queue>
#include <sys/time.h>

#include "Zoltan2_Algorithm.hpp"
#include "Zoltan2_GraphModel.hpp"
#include "Zoltan2_ColoringSolution.hpp"
#include "Zoltan2_Util.hpp"
#include "Zoltan2_TPLTraits.hpp"
#include "Zoltan2_AlltoAll.hpp"


#include "Tpetra_Core.hpp"
#include "Teuchos_RCP.hpp"
#include "Tpetra_Import.hpp"
#include "Tpetra_FEMultiVector.hpp"

#include "Kokkos_Core.hpp"
#include "KokkosSparse_CrsMatrix.hpp"
#include "KokkosKernels_Handle.hpp"
#include "KokkosKernels_IOUtils.hpp"
#include "KokkosGraph_Distance2Color.hpp"
#include "KokkosGraph_Distance2ColorHandle.hpp"

/////////////////////////////////////////////////
//! \file Zoltan2_Distance1_2GhostLayer.hpp
//! \brief A hybrid Distance-2 Coloring Algorithm


namespace Zoltan2{

template <typename Adapter>
class AlgDistance2 : public Algorithm<Adapter> {

  public:
    
    using lno_t = typename Adapter::lno_t;
    using gno_t = typename Adapter::gno_t;
    using offset_t = typename Adapter::offset_t;
    using scalar_t = typename Adapter::scalar_t;
    using base_adapter_t = typename Adapter::base_adapter_t;
    using map_t = Tpetra::Map<lno_t,gno_t>;
    using femv_scalar_t = int;
    using femv_t = Tpetra::FEMultiVector<femv_scalar_t, lno_t, gno_t>; 
    using device_type = Tpetra::Map<>::device_type;
    using execution_space = Tpetra::Map<>::execution_space;
    using memory_space = Tpetra::Map<>::memory_space;
    double timer(){
      struct timeval tp;
      gettimeofday(&tp, NULL);
      return ((double) (tp.tv_sec) + 1e-6 * tp.tv_usec);
    }
  private:
    
    void buildModel(modelFlag_t &flags);
    void colorInterior(const size_t nVtx,
                       Kokkos::View<lno_t*,device_type> adjs_view,
                       Kokkos::View<offset_t*, device_type> offset_view,
                       Teuchos::RCP<femv_t> femv,
                       bool recolor);
    
    RCP<const base_adapter_t> adapter;
    RCP<GraphModel<base_adapter_t> > model;
    RCP<Teuchos::ParameterList> pl;
    RCP<Environment> env;
    RCP<const Teuchos::Comm<int> > comm;
    int numColors;

  public:
    AlgDistance2(
      const RCP<const base_adapter_t> &adapter_,
      const RCP<Teuchos::ParameterList> &pl_,
      const RCP<Environment> &env_,
      const RCP<const Teuchos::Comm<int> > &comm_)
    : adapter(adapter_), pl(pl_), env(env_), comm(comm_){
      //std::cout<<comm->getRank()<<": constructing coloring class...\n";
      numColors = 4;
      modelFlag_t flags;
      flags.reset();
      buildModel(flags);
    }
    

    void constructSecondGhostLayer(std::vector<gno_t>& ownedPlusGhosts, //this argument changes
                                   const std::vector<int>& owners,
                                   ArrayView<const gno_t> adjs,
                                   ArrayView<const offset_t> offsets,
                                   RCP<const map_t> mapOwned,
                                   std::vector< gno_t>& adjs_2GL,
                                   std::vector< offset_t>& offsets_2GL) {
      std::vector<int> sendcounts(comm->getSize(),0);
      std::vector<gno_t> sdispls(comm->getSize()+1,0);
      //loop through owners, count how many vertices we'll send to each processor
      for(size_t i = 0; i < owners.size(); i++){
        if(owners[i] != comm->getRank()&& owners[i] !=-1) sendcounts[owners[i]]++;
      }
      //construct sdispls (for building sendbuf), and sum the total sendcount
      gno_t sendcount = 0;
      for(int i = 1; i < comm->getSize()+1; i++){
        sdispls[i] = sdispls[i-1] + sendcounts[i-1];
        sendcount += sendcounts[i-1];
      }
      std::vector<gno_t> idx(comm->getSize(),0);
      for(int i = 0; i < comm->getSize(); i++){
        idx[i]=sdispls[i];
      }
      //construct sendbuf to send GIDs to owning processes
      std::vector<gno_t> sendbuf(sendcount,0);
      for(size_t i = offsets.size()-1; i < owners.size(); i++){
        if(owners[i] != comm->getRank() && owners[i] != -1){
          sendbuf[idx[owners[i]]++] = ownedPlusGhosts[i];
        }
      }
      
      //communicate GIDs to owners
      Teuchos::ArrayView<int> sendcounts_view = Teuchos::arrayViewFromVector(sendcounts);
      Teuchos::ArrayView<gno_t> sendbuf_view = Teuchos::arrayViewFromVector(sendbuf);
      Teuchos::ArrayRCP<gno_t>  recvbuf;
      std::vector<int> recvcounts(comm->getSize(),0);
      Teuchos::ArrayView<int> recvcounts_view = Teuchos::arrayViewFromVector(recvcounts);
      Zoltan2::AlltoAllv<gno_t>(*comm, *env, sendbuf_view, sendcounts_view, recvbuf, recvcounts_view);
      std::cout<<comm->getRank()<<": completed GID communication to owners\n"; 
      //replace entries in recvGIDs with their degrees
      gno_t recvcounttotal = 0;
      std::vector<int> rdispls(comm->getSize()+1,0);
      for(size_t i = 1; i<recvcounts.size()+1; i++){
        rdispls[i] = rdispls[i-1] + recvcounts[i-1];
        recvcounttotal += recvcounts[i-1];
      }
      //send back the degrees to the requesting processes,
      //build the adjacency counts
      std::vector<offset_t> sendDegrees(recvcounttotal,0);
      gno_t adj_len = 0;
      std::vector<int> adjsendcounts(comm->getSize(),0);
      for(int i = 0; i < comm->getSize(); i++){
        adjsendcounts[i] = 0;
        for(int j = rdispls[i]; j < rdispls[i+1]; j++){
          lno_t lid = mapOwned->getLocalElement(recvbuf[j]);
          offset_t degree = offsets[lid+1] - offsets[lid];
          sendDegrees[j] = degree;
          adj_len +=degree;
          adjsendcounts[i] += degree; 
        }
      }
      //communicate the degrees back to the requesting processes
      Teuchos::ArrayView<offset_t> sendDegrees_view = Teuchos::arrayViewFromVector(sendDegrees);
      Teuchos::ArrayRCP<offset_t> recvDegrees;
      std::vector<int> recvDegreesCount(comm->getSize(),0);
      Teuchos::ArrayView<int> recvDegreesCount_view = Teuchos::arrayViewFromVector(recvDegreesCount);
      Zoltan2::AlltoAllv<offset_t>(*comm, *env, sendDegrees_view, recvcounts_view, recvDegrees, recvDegreesCount_view);
      std::cout<<comm->getRank()<<": completed degree communication to owners\n";
      //calculate number of rounds of AlltoAllv's are necessary on this process
      std::cout<<comm->getRank()<<": calculating number of rounds\n";
      int rounds = 1;
      for(int i = 0; i < comm->getSize(); i++){
        if( adjsendcounts[i]*sizeof(gno_t)/ INT_MAX > rounds){
          rounds = (adjsendcounts[i]*sizeof(gno_t)/ INT_MAX)+1;
        }
      }
      
      //see what the max number of rounds will be, so each process can participate
      int max_rounds = 0;
      Teuchos::reduceAll<int>(*comm, Teuchos::REDUCE_MAX, 1, &rounds, &max_rounds);
      std::cout<<comm->getRank()<<": done calculating number of rounds, max_rounds = "<<max_rounds<<"\n";
      //if(max_rounds > 0){
      //determine how many vertices will be sent to each process in each round
      std::cout<<comm->getRank()<<": calculating how many vertices will be sent to each process per round\n";
      uint64_t**per_proc_round_adj_sums = new uint64_t*[max_rounds+1];
      uint64_t**per_proc_round_vtx_sums = new uint64_t*[max_rounds+1];
      for(int i = 0; i < max_rounds; i++){
        per_proc_round_adj_sums[i] = new uint64_t[comm->getSize()];
        per_proc_round_vtx_sums[i] = new uint64_t[comm->getSize()];
        for(int j = 0; j < comm->getSize(); j++){
          per_proc_round_adj_sums[i][j] = 0;
          per_proc_round_vtx_sums[i][j] = 0;
        }
      }
      
      for(int proc_to_send = 0; proc_to_send < comm->getSize(); proc_to_send++){
        int curr_round = 0;
        for(int j = sdispls[proc_to_send]; j < sdispls[proc_to_send+1]; j++){
          if((per_proc_round_adj_sums[curr_round][proc_to_send] + recvDegrees[j])*sizeof(gno_t) > INT_MAX){
            curr_round++;
            if(curr_round >= max_rounds){
              std::cout<<comm->getRank()<<": curr_round is "<<curr_round<<" and max_rounds is "<<max_rounds<<"\n";
            }
          }
          per_proc_round_adj_sums[curr_round][proc_to_send] += recvDegrees[j];
          per_proc_round_vtx_sums[curr_round][proc_to_send]++;
        }
      }
      for(int i = 0; i < comm->getSize(); i++){
        for(int j = 0; j < max_rounds; j++){
          std::cout<<comm->getRank()<<": receiving "<<per_proc_round_vtx_sums[j][i]<<" vertices from proc "<<i<<" in round "<<j<<"\n";
        }
      }
      std::cout<<comm->getRank()<<": done calculating vertex sums per process per round\n";
      std::cout<<comm->getRank()<<": reorganizing sent GIDs in the order they'll be received\n";
      //remap the GIDs to account for the rounds of communication
      gno_t*** recv_GID_per_proc_per_round = new gno_t**[max_rounds+1];
      for(int i = 0; i < max_rounds; i ++){
        recv_GID_per_proc_per_round[i] = new gno_t*[comm->getSize()];
        for(int j = 0; j < comm->getSize(); j++){
          recv_GID_per_proc_per_round[i][j] = new gno_t[sendcounts[j]];
          for(int k = 0; k < sendcounts[j]; k++){
            recv_GID_per_proc_per_round[i][j][k] = 0;
          }
        }
      }
      std::cout<<comm->getRank()<<": created recv_GID schedule per proc\n"; 
      for(int i = 0; i < comm->getSize(); i++){
        int curr_round = 0;
        int curr_idx = 0;
        for(int j = sdispls[i]; j < sdispls[i+1]; j++){
          if(curr_idx > per_proc_round_vtx_sums[curr_round][i]){
            curr_round++;
            curr_idx = 0;
          }
          if(curr_round >=max_rounds) {
            std::cout<<comm->getSize()<<": curr_round "<<curr_round<<" >= "<<max_rounds<<"\n";
            exit(0);
          }
          if(curr_idx >= sendcounts[i]){ 
            std::cout<<comm->getSize()<<": curr_idx is out of bounds\n";
            exit(0);
          }
          recv_GID_per_proc_per_round[curr_round][i][curr_idx++] = j;
        }
      }
      std::cout<<comm->getRank()<<": assigned schedule\n";
      std::vector<gno_t>   final_gid_vec(sendcount,0);
      std::vector<offset_t>final_degree_vec(sendcount,0); 
      gno_t reorder_idx = 0;
      for(int i = 0; i < max_rounds; i++){
        for(int j = 0; j < comm->getSize(); j++){
          for(int k = 0; k < per_proc_round_vtx_sums[i][j]; k++){
            final_gid_vec[reorder_idx] = sendbuf[recv_GID_per_proc_per_round[i][j][k]];
            final_degree_vec[reorder_idx++] = recvDegrees[recv_GID_per_proc_per_round[i][j][k]];
          }
        }
      }
      bool reorganized = false;
      for(int i = 0; i < sendcount; i ++){
        if(final_gid_vec[i] != sendbuf[i]) reorganized = true;
      }
      if(!reorganized && (max_rounds > 1)) std::cout<<comm->getRank()<<": did not reorganize GIDs, but probably should have\n";
      if(reorganized && (max_rounds == 1)) std::cout<<comm->getRank()<<": reorganized GIDs, but probably should not have\n";
      std::cout<<comm->getRank()<<": done reorganizing, doing final mapping\n";
      //remap the GIDs so we receive the adjacencies in the same order as the current processes LIDs
      for(gno_t i = 0; i < sendcount; i++){
        ownedPlusGhosts[i+offsets.size()-1] = final_gid_vec[i];
      }
      std::cout<<comm->getRank()<<": done remapping\n";
      //at this point, final_gid_vec should have the GIDs in the order we'll receive the adjacencies.
      //similarly, final_degree_vec should have the degrees in the order we'll receive the adjacencies.
      //construct offsets with the received vertex degrees
      std::cout<<comm->getRank()<<": creating ghost offsets\n";
      std::vector<offset_t> ghost_offsets(sendcount+1,0);
      std::vector<lno_t> send_adjs(adj_len,0);
      for(int i = 1; i < sendcount+1; i++){
        ghost_offsets[i] = ghost_offsets[i-1] + final_degree_vec[i-1];
      }
      std::cout<<comm->getRank()<<": done creating offsets\n";
      //build adjacency lists to send back
      //use recvGIDs to look up adjacency information
      std::cout<<comm->getRank()<<": creating total list of adjacencies to send\n";
      offset_t adjidx = 0;
      for (int i = 0; i < recvcounttotal; i++){
        lno_t lid = mapOwned->getLocalElement(recvbuf[i]);
        for(offset_t j = offsets[lid]; j < offsets[lid+1]; j++){
          send_adjs[adjidx++] = adjs[j];
        }
      }
      //count how many adjacencies we will receive
      offset_t recvadjscount = 0;
      for(int i = 0; i < recvDegrees.size(); i++){
        recvadjscount+= final_degree_vec[i];
      }
      std::cout<<comm->getRank()<<"; done creating total send adjs\n";
      uint64_t* curr_idx_per_proc = new uint64_t[comm->getSize()];
      for(int i = 0; i < comm->getSize(); i++) curr_idx_per_proc[i] = rdispls[i]; 
      std::cout<<comm->getRank()<<": starting rounds of communication\n";
      for(int round = 0; round < max_rounds; round++){
        //communicate adjacencies back to requesting processes, in chunks that fit Zoltan2_Alltoallv's message size constraints.
        //create the send_adj vector for this round
        std::cout<<comm->getRank()<<": round "<<round<<" create send buffer\n";
        std::vector<gno_t> send_adj;
        std::vector<int> send_adj_counts(comm->getSize(),0);
        for(int curr_proc = 0; curr_proc < comm->getSize(); curr_proc++){
          uint64_t curr_adj_sum = 0;
          while( curr_idx_per_proc[curr_proc] < rdispls[curr_proc+1]){
            lno_t lid = mapOwned->getLocalElement(recvbuf[curr_idx_per_proc[curr_proc]++]);
            if((curr_adj_sum + (offsets[lid+1]-offsets[lid]))*sizeof(gno_t) >= INT_MAX){
              break;
            }
            curr_adj_sum += (offsets[lid+1] - offsets[lid]);
            for(offset_t j = offsets[lid]; j < offsets[lid+1]; j++){
              send_adj.push_back(adjs[j]);
            }
          }
          send_adj_counts[curr_proc] = curr_adj_sum;
        }
        std::cout<<comm->getRank()<<": round "<<round<<" done creating send buffer\n";
        Teuchos::ArrayView<gno_t> send_adjs_view = Teuchos::arrayViewFromVector(send_adj);
        Teuchos::ArrayView<int> adjsendcounts_view = Teuchos::arrayViewFromVector(send_adj_counts);
        Teuchos::ArrayRCP<gno_t> ghost_adjs;
        std::cout<<comm->getRank()<<": round "<<round<<" calculating total received this round\n";
        uint64_t recv_adjs_count = 0;
        for(int i = 0; i < comm->getSize(); i++){
          recv_adjs_count += per_proc_round_adj_sums[round][i]; 
        }
        std::cout<<comm->getRank()<<": round "<<round<<" done calculating received total\n";
        std::vector<int> adjrecvcounts(comm->getSize(),0);
        Teuchos::ArrayView<int> adjsrecvcounts_view = Teuchos::arrayViewFromVector(adjrecvcounts);
        std::cout<<comm->getRank()<<": round "<<round<<" going to communicate\n";
        Zoltan2::AlltoAllv<gno_t>(*comm, *env, send_adjs_view, adjsendcounts_view, ghost_adjs, adjsrecvcounts_view);
        std::cout<<comm->getRank()<<": completed adjacency communication to owners\n";
        //build the adjacencies and offsets for the second ghost layer
        for(offset_t i = 0; i < ghost_adjs.size(); i ++){
          adjs_2GL.push_back(ghost_adjs[i]);
        }
      }
      for(int i = 0; i < sendcount+1; i++){
        offsets_2GL.push_back(ghost_offsets[i]);
      }
      /*for(int i = 0; i < offsets_2GL.size()-1; i++){
        gno_t gid = ownedPlusGhosts[i+offsets.size()-1];
        std::cout<<comm->getRank()<<": vertex "<<gid<<" neighbors ";
        for(int j = offsets_2GL[i]; j < offsets_2GL[i+1]; j++){
          std::cout<<adjs_2GL[j]<<" ";
        }
        std::cout<<"\n";
      }*/
    }
    double doOwnedToGhosts(RCP<const map_t> mapOwnedPlusGhosts,
                         size_t nVtx,
                         ArrayView<int> owners,
                         Kokkos::View<int*, device_type>& colors){
      int nprocs = comm->getSize();
      int* sendcnts = new int[nprocs];
      int* recvcnts = new int[nprocs];
      for(int i = 0; i < nprocs; i++){
        sendcnts[i] = 0;
        recvcnts[i] = 0;
      }
      //loop through owners, count how many vertices we'll send to each processor
      for(size_t i=0; i < owners.size(); i++){
        if(owners[i] != comm->getRank() && owners[i] != -1) sendcnts[owners[i]]++;
      }
      int status = MPI_Alltoall(sendcnts,1,MPI_INT,recvcnts,1,MPI_INT,MPI_COMM_WORLD);

      int* sdispls = new int[nprocs];
      int* rdispls = new int[nprocs];
      //construct sdispls (for building sendbuf), and sum the total sendcount
      sdispls[0] = 0;
      rdispls[0] = 0;
      gno_t sendsize = 0;
      gno_t recvsize = 0;
      int* sentcount = new int[nprocs];
      for(int i = 1; i < comm->getSize()+1; i++){
        sdispls[i] = sdispls[i-1] + sendcnts[i-1];
        rdispls[i] = rdispls[i-1] + recvcnts[i-1];
        sendsize += sendcnts[i-1];
        recvsize += recvcnts[i-1];
        sentcount[i-1] = 0;
        //std::cout<<comm->getRank()<<": sending "<<sendcounts[i-1]<<" GIDs to proc "<<i-1<<"\n";
      }
      int* sendbuf = new int[sendsize];
      int* recvbuf = new int[recvsize];

      for(size_t i = 0; i < owners.size(); i++){
        if(owners[i] != comm->getRank() && owners[i] != -1){
          int idx = sdispls[owners[i]] + sentcount[owners[i]]++;
          sendbuf[idx] = mapOwnedPlusGhosts->getGlobalElement(i+nVtx);
        }
      }

      double comm_total = 0.0;
      double comm_temp = timer();
      status = MPI_Alltoallv(sendbuf, sendcnts, sdispls, MPI_INT, recvbuf,recvcnts,rdispls,MPI_INT,MPI_COMM_WORLD);
      comm_total += timer() - comm_temp;

      int* recvColors = new int[sendsize];

      for(int i = 0; i < comm->getSize(); i++){
        for(int j = rdispls[i]; j < rdispls[i+1]; j++){
          lno_t lid = mapOwnedPlusGhosts->getLocalElement(recvbuf[j]);
          recvbuf[j] = colors(lid);
        }
      }

      comm_temp = timer();
      status = MPI_Alltoallv(recvbuf, recvcnts,rdispls,MPI_INT, recvColors, sendcnts, sdispls,MPI_INT,MPI_COMM_WORLD);
      comm_total += timer() - comm_temp;

      for(int i = 0; i < sendsize; i++){
        colors(mapOwnedPlusGhosts->getLocalElement(sendbuf[i])) = recvColors[i];
        //std::cout<<comm->getRank()<<": global vert "<<sendbuf[i]<<" is now color  "<<recvColors[i]<<"\n";
      }
      return comm_total;
      /*std::vector<int> sendcounts(comm->getSize(), 0);
      std::vector<gno_t> sdispls(comm->getSize()+1, 0);
      //loop through owners, count how many vertices we'll send to each processor
      for(size_t i = 0; i < owners.size(); i++){
        if(owners[i] != comm->getRank() && owners[i] != -1) sendcounts[owners[i]]++;
      }
      //construct sdispls (for building sendbuf), and sum the total sendcount
      gno_t sendcount = 0;
      for(int i = 1; i < comm->getSize()+1; i++){
        sdispls[i] = sdispls[i-1] + sendcounts[i-1];
        sendcount+= sendcounts[i-1];
      }
      std::vector<gno_t> idx(comm->getSize(), 0);
      for(int i = 0; i < comm->getSize(); i++){
        idx[i] = sdispls[i];
      }
      //construct sendbuf to send GIDs to owning processes
      std::vector<gno_t> sendbuf(sendcount,0);
      for(size_t i = 0; i < owners.size(); i++){
        if(owners[i] != comm->getRank() && owners[i] != -1){
          sendbuf[idx[owners[i]]++] = mapOwnedPlusGhosts->getGlobalElement(i+nVtx);
        }
      }
      
      //communicate GIDs to owners
      Teuchos::ArrayView<int> sendcounts_view = Teuchos::arrayViewFromVector(sendcounts);
      Teuchos::ArrayView<gno_t> sendbuf_view = Teuchos::arrayViewFromVector(sendbuf);
      Teuchos::ArrayRCP<gno_t> recvbuf;
      std::vector<int> recvcounts(comm->getSize(),0);
      Teuchos::ArrayView<int> recvcounts_view = Teuchos::arrayViewFromVector(recvcounts);
      double comm_total = 0.0;
      double comm_temp = timer();
      Zoltan2::AlltoAllv<gno_t>(*comm, *env, sendbuf_view, sendcounts_view, recvbuf, recvcounts_view);
      comm_total += timer() - comm_temp;
      //replace entries in recvbuf with local color
      gno_t recvcounttotal = 0;
      std::vector<int> rdispls(comm->getSize()+1,0);
      for(size_t i = 1; i < recvcounts.size()+1; i++){
        rdispls[i] = rdispls[i-1] + recvcounts[i-1];
        recvcounttotal += recvcounts[i-1];
      }
      
      //set colors to send back to requesting processes
      std::vector<int> sendColors(recvcounttotal,0);
      gno_t color_len = 0;
      std::vector<int> colorSendCounts(comm->getSize(),0);
      for(int i = 0; i < comm->getSize(); i++){
        colorSendCounts[i]=rdispls[i+1] - rdispls[i];
        for(int j = rdispls[i]; j < rdispls[i+1]; j++){
          lno_t lid = mapOwnedPlusGhosts->getLocalElement(recvbuf[j]);
          sendColors[j] = colors(lid);
        }
      }
      //communicate colors back to requesting processes
      Teuchos::ArrayView<int> sendColors_view = Teuchos::arrayViewFromVector(sendColors);
      Teuchos::ArrayRCP<int> recvColors;
      std::vector<int> recvColorsCount(comm->getSize(),0);
      Teuchos::ArrayView<int> recvColorsCount_view = Teuchos::arrayViewFromVector(recvColorsCount);
      comm_temp = timer();
      Zoltan2::AlltoAllv<int>(*comm, *env, sendColors_view, recvcounts_view, recvColors, recvColorsCount_view);
      comm_total += timer() - comm_temp;
      //set colors of ghosts to newly received colors
      for(int i = 0; i < recvColors.size(); i++){
        colors(mapOwnedPlusGhosts->getLocalElement(sendbuf[i])) = recvColors[i];
      }
      return comm_total;*/
    } 
    //Main entry point for graph coloring
    void color( const RCP<ColoringSolution<Adapter> > &solution){
      std::cout<<comm->getRank()<<": inside coloring\n";
      //convert from global graph to local graph
      ArrayView<const gno_t> vtxIDs;
      ArrayView<StridedData<lno_t, scalar_t> > vwgts;
      size_t nVtx = model->getVertexList(vtxIDs, vwgts);
      // the weights are not used at this point.
      
      ArrayView<const gno_t> adjs;
      ArrayView<const offset_t> offsets;
      ArrayView<StridedData<lno_t, scalar_t> > ewgts;
      size_t nEdge = model->getEdgeList(adjs, offsets, ewgts);      
      

      std::unordered_map<gno_t,lno_t> globalToLocal;
      std::vector<gno_t> ownedPlusGhosts;
      std::vector<int> owners;
      for(int i = 0; i < vtxIDs.size(); i++){
        globalToLocal[vtxIDs[i]] = i;
        ownedPlusGhosts.push_back(vtxIDs[i]);
        owners.push_back(comm->getRank());
      }

      int nGhosts = 0;
      std::vector<lno_t> local_adjs;
      for(int i = 0; i < adjs.size(); i++){
        if(globalToLocal.count(adjs[i])==0){
          ownedPlusGhosts.push_back(adjs[i]);
          globalToLocal[adjs[i]] = vtxIDs.size()+nGhosts;
          nGhosts++;
        }
        local_adjs.push_back(globalToLocal[adjs[i]]);
      }
      
      Tpetra::global_size_t dummy = Teuchos::OrdinalTraits
                                           <Tpetra::global_size_t>::invalid();
      RCP<const map_t> mapOwned = rcp(new map_t(dummy, vtxIDs, 0, comm));
      std::vector<gno_t> ghosts;
      std::vector<int> ghostowners;
      for(size_t i = nVtx; i < nVtx+nGhosts; i++){
        ghosts.push_back(ownedPlusGhosts[i]);
        ghostowners.push_back(-1);
      }
      
      //get the owners of the vertices
      ArrayView<int> owningProcs = Teuchos::arrayViewFromVector(ghostowners);
      ArrayView<const gno_t> gids = Teuchos::arrayViewFromVector(ghosts);
      Tpetra::LookupStatus ls = mapOwned->getRemoteIndexList(gids, owningProcs);
      
      for(size_t i = 0; i < ghostowners.size(); i++){
        owners.push_back(ghostowners[i]);
      }
      std::cout<<comm->getRank()<<": n_local = "<<nVtx<<"\n"; 
      //use the mapOwned to find the owners of ghosts
      std::vector< gno_t> first_layer_ghost_adjs;
      std::vector< offset_t> first_layer_ghost_offsets;
      constructSecondGhostLayer(ownedPlusGhosts,owners, adjs, offsets, mapOwned,
                                first_layer_ghost_adjs, first_layer_ghost_offsets);
      
      //we potentially reordered the local IDs of the ghost vertices, so we need
      //to re-insert the GIDs into the global to local ID mapping.
      globalToLocal.clear();
      for(size_t i = 0; i < ownedPlusGhosts.size(); i++){
        globalToLocal[ownedPlusGhosts[i]] = i;
      }
      
      for(int i = 0 ; i < adjs.size(); i++){
        local_adjs[i] = globalToLocal[adjs[i]];
      }
      //at this point, we have ownedPlusGhosts with 1layer ghosts' GIDs.
      //need to add 2layer ghost GIDs, and add them to the map.
      size_t n2Ghosts = 0;
      std::vector<lno_t> local_ghost_adjs;
      for(size_t i = 0; i< first_layer_ghost_adjs.size(); i++ ){
        if(globalToLocal.count(first_layer_ghost_adjs[i]) == 0){
          ownedPlusGhosts.push_back(first_layer_ghost_adjs[i]);
          globalToLocal[first_layer_ghost_adjs[i]] = vtxIDs.size() + nGhosts + n2Ghosts;
          n2Ghosts++;
        }
        local_ghost_adjs.push_back(globalToLocal[first_layer_ghost_adjs[i]]);
      }
      dummy = Teuchos::OrdinalTraits <Tpetra::global_size_t>::invalid();
      RCP<const map_t> mapWithCopies = rcp(new map_t(dummy,
                                           Teuchos::arrayViewFromVector(ownedPlusGhosts),
                                           0, comm));
      using import_t = Tpetra::Import<lno_t, gno_t>;
      Teuchos::RCP<import_t> importer = rcp(new import_t(mapOwned,
                                                            mapWithCopies));
      Teuchos::RCP<femv_t> femv = rcp(new femv_t(mapOwned,
                                                    importer, 1, true));
      //Create random numbers seeded on global IDs, as in AlgHybridGMB.
      //This may or may not have an effect on this algorithm, but we
      //might as well see.
      std::vector<int> rand(ownedPlusGhosts.size());
      for(size_t i = 0; i < rand.size(); i++){
        std::srand(ownedPlusGhosts[i]);
        rand[i] = std::rand();
      }

      

      Teuchos::ArrayView<const lno_t> local_adjs_view = Teuchos::arrayViewFromVector(local_adjs);
      Teuchos::ArrayView<const offset_t> ghost_offsets = Teuchos::arrayViewFromVector(first_layer_ghost_offsets);
      Teuchos::ArrayView<const lno_t> ghost_adjacencies = Teuchos::arrayViewFromVector(local_ghost_adjs);
      std::vector<int> ghostOwners(ownedPlusGhosts.size() - nVtx);
      std::vector<gno_t> ghosts_vec(ownedPlusGhosts.size() - nVtx);
      for(int i = nVtx; i < ownedPlusGhosts.size(); i++) ghosts_vec[i-nVtx] = ownedPlusGhosts[i];
      ArrayView<int> ghostOwners_view = Teuchos::arrayViewFromVector(ghostOwners);
      ArrayView<const gno_t> ghostGIDs = Teuchos::arrayViewFromVector(ghosts_vec);
      Tpetra::LookupStatus ls2 = mapOwned->getRemoteIndexList(ghostGIDs, ghostOwners_view);
      //test communication consistency:
      /*for(int i = 0; i < 100; i++){
        comm->barrier();
        femv->switchActiveMultiVector();
        double comm_tmp = timer();
        femv->doOwnedToOwnedPlusShared(Tpetra::REPLACE);
        double comm_time = timer() - comm_tmp;
        femv->switchActiveMultiVector();
        std::cout<<comm->getRank()<<": FEMV Comm "<<i<<" time: "<<comm_time<<"\n";
      }
      Kokkos::View<int**, Kokkos::LayoutLeft> femvColors = femv->template getLocalView<memory_space>();
      Kokkos::View<int*, device_type> femv_colors = subview(femvColors, Kokkos::ALL, 0);
      for(int i = 0; i < 100; i++){
        comm->barrier();
        double comm_time = doOwnedToGhosts(mapWithCopies, nVtx, owners, femv_colors);
        std::cout<<comm->getRank()<<": MPI Comm "<<i<<" time :"<<comm_time<<"\n";
      }*/
      //call the coloring algorithm
      twoGhostLayer(nVtx, nVtx+nGhosts, local_adjs_view, offsets, ghost_adjacencies, ghost_offsets,
                    femv, ownedPlusGhosts, globalToLocal, rand,ghostOwners_view, mapWithCopies);
      
      //copy colors to the output array
      ArrayRCP<int> colors = solution->getColorsRCP();
      for(size_t i=0; i<nVtx; i++){
         colors[i] = femv->getData(0)[i];
      }

    }

    void twoGhostLayer(const size_t n_local, const size_t n_total,
                       const Teuchos::ArrayView<const lno_t>& adjs,
                       const Teuchos::ArrayView<const offset_t>& offsets,
                       const Teuchos::ArrayView<const lno_t>& ghost_adjs,
                       const Teuchos::ArrayView<const offset_t>& ghost_offsets,
                       const Teuchos::RCP<femv_t>& femv,
                       const std::vector<gno_t>& gids, 
                       const std::unordered_map<gno_t,lno_t>& globalToLocal,
                       const std::vector<int>& rand,
                       ArrayView<int> owners,
                       RCP<const map_t> mapWithCopies){
      std::cout<<comm->getRank()<<": inside twoGhostLayer\n";
      double total_time = 0.0;
      double interior_time = 0.0;
      double comm_time = 0.0;
      double comp_time = 0.0;
      double recoloring_time = 0.0;
      double conflict_detection = 0.0;
      //get the color view from the FEMultiVector
      Kokkos::View<int**, Kokkos::LayoutLeft> femvColors = femv->template getLocalView<memory_space>();
      Kokkos::View<int*, device_type> femv_colors = subview(femvColors, Kokkos::ALL, 0);
      Kokkos::View<offset_t*, device_type> host_offsets("Host Offset View", offsets.size());
      Kokkos::View<lno_t*, device_type> host_adjs("Host Adjacencies View", adjs.size());
      for(Teuchos_Ordinal i = 0; i < offsets.size(); i++) host_offsets(i) = offsets[i];
      for(Teuchos_Ordinal i = 0; i < adjs.size(); i++) host_adjs(i) = adjs[i];

      //give the entire local graph to KokkosKernels to color
      std::cout<<comm->getRank()<<" coloring interior\n";
      comm->barrier();
      interior_time = timer();
      this->colorInterior(n_local, host_adjs, host_offsets, femv, false);
      interior_time = timer() - interior_time;
      total_time = interior_time;
      comp_time = interior_time;
      comm->barrier();    
      std::cout<<comm->getRank()<<" communicating\n";
      //communicate the initial coloring.
      //femv->switchActiveMultiVector();
      //double comm_temp = timer();
      //femv->doOwnedToOwnedPlusShared(Tpetra::REPLACE);
      comm_time = doOwnedToGhosts(mapWithCopies, n_local, owners, femv_colors);
      //comm_time = timer() - comm_temp;
      total_time += comm_time;
      //femv->switchActiveMultiVector();
      
      std::cout<<comm->getRank()<<" building distributed graph\n";
      //create the graph structures which allow KokkosKernels to recolor the conflicting vertices
      Kokkos::View<offset_t*, Tpetra::Map<>::device_type> dist_degrees("Owned+Ghost degree view",rand.size());
      for(Teuchos_Ordinal i = 0; i < adjs.size(); i++) dist_degrees(adjs[i])++;
      for(Teuchos_Ordinal i = 0; i < ghost_adjs.size(); i++) dist_degrees(ghost_adjs[i])++;
      for(Teuchos_Ordinal i = 0; i < offsets.size()-1; i++) dist_degrees(i) = offsets[i+1] - offsets[i];
      for(Teuchos_Ordinal i = 0; i < ghost_offsets.size()-1; i++) dist_degrees(i+n_local) = ghost_offsets[i+1] - ghost_offsets[i];
      
      Kokkos::View<offset_t*, Tpetra::Map<>::device_type> dist_offsets("Owned+Ghost Offset view", rand.size()+1);
      dist_offsets(0) = 0;
      offset_t total_adjs = 0;
      for(size_t i = 1; i < rand.size()+1; i++){
        dist_offsets(i) = dist_degrees(i-1) + dist_offsets(i-1);
        total_adjs += dist_degrees(i-1);
      }
      Kokkos::View<lno_t*, Tpetra::Map<>::device_type> dist_adjs("Owned+Ghost adjacency view", total_adjs);
      for(size_t i = 0; i < rand.size(); i++){
        dist_degrees(i) = 0;
      }
      for(Teuchos_Ordinal i = 0; i < adjs.size(); i++) dist_adjs(i) = adjs[i];
      for(Teuchos_Ordinal i = adjs.size(); i < adjs.size() + ghost_adjs.size(); i++) dist_adjs(i) = ghost_adjs[i-adjs.size()];
      for(Teuchos_Ordinal i = 0; i < ghost_offsets.size()-1; i++){
        for(offset_t j = ghost_offsets[i]; j < ghost_offsets[i+1]; j++){
         if((size_t)ghost_adjs[j] >= n_total){
            dist_adjs(dist_offsets(ghost_adjs[j]) + dist_degrees(ghost_adjs[j])) = i + n_local;
            dist_degrees(ghost_adjs[j])++;
          }
        }
      }

      std::cout<<comm->getRank()<<": creating Kokkos views\n";
      //we can find all the conflicts with one loop through the ghost vertices.
      
      //this view represents how many conflicts were found
      Kokkos::View<gno_t[1], device_type> recoloringSize("Recoloring Queue Size");
      recoloringSize(0) = 0;
      //keep an atomic version so that we can increment from multiple threads
      Kokkos::View<gno_t[1], device_type, Kokkos::MemoryTraits<Kokkos::Atomic> > recoloringSize_atomic = recoloringSize;

      //create views for the ghost adjacencies, as they can detect all conflicts.
      Kokkos::View<offset_t*, device_type> ghost_offset_view("Ghost Offsets", ghost_offsets.size());
      Kokkos::View<lno_t*, device_type> ghost_adjs_view("Ghost Adjacencies", ghost_adjs.size());
      for(Teuchos_Ordinal i = 0; i < ghost_offsets.size(); i++) ghost_offset_view(i) = ghost_offsets[i];
      for(Teuchos_Ordinal i = 0; i < ghost_adjs.size(); i++) ghost_adjs_view(i) = ghost_adjs[i];


      //create a view for the tie-breaking numbers.
      Kokkos::View<int*, device_type> rand_view("Random View", rand.size());
      for(size_t i = 0; i < rand.size(); i ++) rand_view(i) = rand[i];
      
      Kokkos::View<gno_t*, device_type> gid_view("GIDs", gids.size());
      for(size_t i = 0; i < gids.size(); i++) gid_view(i) = gids[i];
      comm->barrier();
      double temp = timer();
      Kokkos::parallel_for(offsets.size()-1, KOKKOS_LAMBDA(const uint64_t& i){
        const lno_t curr_lid = i;
        const int curr_color = femv_colors(curr_lid);
        const size_t vid_d1_adj_begin = host_offsets(i);
        const size_t vid_d1_adj_end   = host_offsets(i+1);
        for(size_t vid_d1_adj = vid_d1_adj_begin; vid_d1_adj < vid_d1_adj_end; vid_d1_adj++){
          lno_t vid_d1 = host_adjs(vid_d1_adj);
          if( vid_d1 != curr_lid && femv_colors(vid_d1) == curr_color){
            if(rand_view(curr_lid) < rand_view(vid_d1)){
              femv_colors(curr_lid) = 0;
              recoloringSize_atomic(0)++;
              return;
            } else if (rand_view(vid_d1) < rand_view(curr_lid)){
              femv_colors(vid_d1) = 0;
              recoloringSize_atomic(0)++;
            } else{
              if(gid_view(curr_lid) >= gid_view(vid_d1)){
                femv_colors(curr_lid) = 0;
                recoloringSize_atomic(0)++;
                return;
              } else {
                femv_colors(vid_d1) = 0;
                recoloringSize_atomic(0)++;
              }
            }
          }
          size_t d2_adj_begin = 0;
          size_t d2_adj_end   = 0;
          bool is_owned = true;
          if(vid_d1 < n_local){
            d2_adj_begin = host_offsets(vid_d1);
            d2_adj_end   = host_offsets(vid_d1+1);
          } else if (vid_d1 >=n_local && vid_d1 < n_total){
            d2_adj_begin = ghost_offset_view(vid_d1 - n_local);
            d2_adj_end   = ghost_offset_view(vid_d1 - n_local +1);
            is_owned = false;
          }
          for(size_t vid_d2_adj = d2_adj_begin; vid_d2_adj < d2_adj_end; vid_d2_adj++){
            const lno_t vid_d2 = is_owned ? host_adjs(vid_d2_adj) : ghost_adjs_view(vid_d2_adj);
            if(curr_lid != vid_d2 && femv_colors(vid_d2) == curr_color){
              if(rand_view(curr_lid) < rand_view(vid_d2)){
                femv_colors(curr_lid) = 0;
                recoloringSize_atomic(0)++;
                return;
              } else if(rand_view(vid_d2) < rand_view(curr_lid)) {
                femv_colors(vid_d2) = 0;
                recoloringSize_atomic(0)++;
              } else {
                if(gid_view(curr_lid) >= gid_view(vid_d2)){
                  femv_colors(curr_lid) = 0;
                  recoloringSize_atomic(0)++;
                  return;
                } else {
                  femv_colors(vid_d2) = 0;
                  recoloringSize_atomic(0)++;
                }
              }
            }
          }
        }
      });
      Kokkos::fence();
      if(comm->getSize() > 1){
        conflict_detection = timer() - temp;
        total_time += conflict_detection;
        comp_time += conflict_detection;
      }
      //ensure that the parallel_for finishes before continuing
      std::cout<<comm->getRank()<<": done checking initial conflicts\n";
     
      //all conflicts detected!

      //variables for statistics
      double totalPerRound[100];
      double commPerRound[100];
      double compPerRound[100];
      double recoloringPerRound[100];
      double conflictDetectionPerRound[100];
      uint64_t vertsPerRound[100];
      uint64_t distributedRounds = 1;
      totalPerRound[0] = interior_time + comm_time + conflict_detection;
      recoloringPerRound[0] = 0;
      commPerRound[0] = comm_time;
      compPerRound[0] = interior_time + conflict_detection;
      conflictDetectionPerRound[0] = conflict_detection;
      recoloringPerRound[0] = 0;
      vertsPerRound[0] = 0;
      //see if recoloring is necessary.
      gno_t totalConflicts = 0;
      gno_t localConflicts = recoloringSize(0);
      Teuchos::reduceAll<int,gno_t>(*comm, Teuchos::REDUCE_SUM, 1, &localConflicts, &totalConflicts);
      bool done = !totalConflicts;
      if(comm->getSize()==1) done = true;
      
      //recolor until no conflicts are left
      while(!done){
        if(distributedRounds < 100){
         // vertsPerRound[distributedRounds++] = recoloringSize(0);
         int localVertsToRecolor = 0;
         for(int i = 0; i < n_local; i++){
           if(femv_colors(i) == 0) localVertsToRecolor++;
         }
         vertsPerRound[distributedRounds] = localVertsToRecolor;
        }
  
     
        double recolor_temp = timer();
        //recolor using KokkosKernels' coloring function 
        this->colorInterior(femv_colors.size(), dist_adjs, dist_offsets, femv, true);
        recoloringPerRound[distributedRounds] = timer() - recolor_temp;
        recoloring_time += recoloringPerRound[distributedRounds];
        total_time += recoloringPerRound[distributedRounds];
        comp_time += recoloringPerRound[distributedRounds];
        compPerRound[distributedRounds] = recoloringPerRound[distributedRounds];
        totalPerRound[distributedRounds] = recoloringPerRound[distributedRounds];
        recoloringSize(0) = 0;
        //communicate the new colors
        comm->barrier();
        //femv->switchActiveMultiVector();
        //double comm_loop_temp = timer();
        //femv->doOwnedToOwnedPlusShared(Tpetra::REPLACE);
        commPerRound[distributedRounds] = doOwnedToGhosts(mapWithCopies,n_local,owners,femv_colors);
        //commPerRound[distributedRounds] = timer() - comm_loop_temp;
        comm_time += commPerRound[distributedRounds];
        totalPerRound[distributedRounds] += commPerRound[distributedRounds];
        total_time += commPerRound[distributedRounds];
        //femv->switchActiveMultiVector();
        comm->barrier();

        double detection_temp = timer();
        Kokkos::parallel_for(offsets.size()-1, KOKKOS_LAMBDA(const uint64_t& i){
          const lno_t curr_lid = i;
          const int curr_color = femv_colors(curr_lid);
          const size_t vid_d1_adj_begin = host_offsets(i);
          const size_t vid_d1_adj_end   = host_offsets(i+1);
          for(size_t vid_d1_adj = vid_d1_adj_begin; vid_d1_adj < vid_d1_adj_end; vid_d1_adj++){
            lno_t vid_d1 = host_adjs(vid_d1_adj);
            if( vid_d1 != curr_lid && femv_colors(vid_d1) == curr_color){
              if(rand_view(curr_lid) < rand_view(vid_d1)){
                femv_colors(curr_lid) = 0;
                recoloringSize_atomic(0)++;
                return;
              } else if (rand_view(vid_d1) < rand_view(curr_lid)){
                femv_colors(vid_d1) = 0;
                recoloringSize_atomic(0)++;
              } else{
                if(gid_view(curr_lid) >= gid_view(vid_d1)){
                  femv_colors(curr_lid) = 0;
                  recoloringSize_atomic(0)++;
                  return;
                } else {
                  femv_colors(vid_d1) = 0;
                  recoloringSize_atomic(0)++;
                }
              }
            }
            size_t d2_adj_begin = 0;
            size_t d2_adj_end   = 0;
            bool is_owned = true;
            if(vid_d1 < n_local){
              d2_adj_begin = host_offsets(vid_d1);
              d2_adj_end   = host_offsets(vid_d1+1);
            } else if (vid_d1 >=n_local && vid_d1 < n_total){
              d2_adj_begin = ghost_offset_view(vid_d1 - n_local);
              d2_adj_end   = ghost_offset_view(vid_d1 - n_local +1);
              is_owned = false;
            }
            for(size_t vid_d2_adj = d2_adj_begin; vid_d2_adj < d2_adj_end; vid_d2_adj++){
              const lno_t vid_d2 = is_owned ? host_adjs(vid_d2_adj) : ghost_adjs_view(vid_d2_adj);
              if(curr_lid != vid_d2 && femv_colors(vid_d2) == curr_color){
                if(rand_view(curr_lid) < rand_view(vid_d2)){
                  femv_colors(curr_lid) = 0;
                  recoloringSize_atomic(0)++;
                  return;
                } else if(rand_view(vid_d2) < rand_view(curr_lid)) {
                  femv_colors(vid_d2) = 0;
                  recoloringSize_atomic(0)++;
                } else {
                  if(gid_view(curr_lid) >= gid_view(vid_d2)){
                    femv_colors(curr_lid) = 0;
                    recoloringSize_atomic(0)++;
                    return;
                  } else {
                    femv_colors(vid_d2) = 0;
                    recoloringSize_atomic(0)++;
                  }
                }
              }
            }
          }
        });
        //ensure the parallel_for finishes before continuing
        Kokkos::fence(); 
        conflictDetectionPerRound[distributedRounds] = timer() - detection_temp;
        conflict_detection += conflictDetectionPerRound[distributedRounds];
        compPerRound[distributedRounds] += conflictDetectionPerRound[distributedRounds];
        totalPerRound[distributedRounds] += conflictDetectionPerRound[distributedRounds];
        total_time += conflictDetectionPerRound[distributedRounds];
        comp_time += conflictDetectionPerRound[distributedRounds];

        distributedRounds++;
        int localDone = recoloringSize(0);
        int globalDone = 0;
        Teuchos::reduceAll<int,int>(*comm, Teuchos::REDUCE_SUM, 1, &localDone, &globalDone);
        std::cout<<comm->getRank()<<": globalDone="<<globalDone<<"\n";
        done = !globalDone;
      
      }//end coloring
      std::cout<<comm->getRank()<<": done recoloring loop, computing statistics\n";
      uint64_t localBoundaryVertices = 0;
      for(int i = 0; i < n_local; i++){
        for(int j = offsets[i]; j < offsets[i+1]; j++){
          if(adjs[j] >= n_local){
            localBoundaryVertices++;
            break;
          }
        }
      }
      
      if(comm->getRank() == 0) printf("did %d rounds of distributed coloring\n", distributedRounds);
      uint64_t totalVertsPerRound[100];
      uint64_t totalBoundarySize = 0;
      double finalTotalPerRound[100];
      double maxRecoloringPerRound[100];
      double minRecoloringPerRound[100];
      double finalCommPerRound[100];
      double finalCompPerRound[100];
      double finalConflictDetectionPerRound[100];
      for(int i = 0; i < 100; i++){
        totalVertsPerRound[i] = 0;
        finalTotalPerRound[i] = 0.0;
        maxRecoloringPerRound[i] = 0.0;
        minRecoloringPerRound[i] = 0.0;
        finalCommPerRound[i] = 0.0;
        finalCompPerRound[i] = 0.0;
        finalConflictDetectionPerRound[i] = 0.0;
      }
      Teuchos::reduceAll<int,uint64_t>(*comm, Teuchos::REDUCE_SUM,1,&localBoundaryVertices,&totalBoundarySize);
      Teuchos::reduceAll<int,uint64_t>(*comm, Teuchos::REDUCE_SUM,100,vertsPerRound,totalVertsPerRound);
      Teuchos::reduceAll<int,double>(*comm, Teuchos::REDUCE_MAX,100,totalPerRound, finalTotalPerRound);
      Teuchos::reduceAll<int,double>(*comm, Teuchos::REDUCE_MAX,100,recoloringPerRound,maxRecoloringPerRound);
      Teuchos::reduceAll<int,double>(*comm, Teuchos::REDUCE_MIN,100,recoloringPerRound,minRecoloringPerRound);
      Teuchos::reduceAll<int,double>(*comm, Teuchos::REDUCE_MAX,100,commPerRound,finalCommPerRound);
      Teuchos::reduceAll<int,double>(*comm, Teuchos::REDUCE_MAX,100,compPerRound,finalCompPerRound);
      Teuchos::reduceAll<int,double>(*comm, Teuchos::REDUCE_MAX,100,conflictDetectionPerRound, finalConflictDetectionPerRound);
      printf("Rank %d: boundary size: %d\n",comm->getRank(),localBoundaryVertices);
      for(int i = 0; i < std::min((int)distributedRounds,100); i++){
        printf("Rank %d: recolor %d vertices in round %d\n",comm->getRank(),vertsPerRound[i],i);
        if(comm->getRank()==0) printf("recolored %d vertices in round %d\n",totalVertsPerRound[i],i);
        if(comm->getRank()==0) printf("total time in round %d: %f\n",i,finalTotalPerRound[i]);
        if(comm->getRank()==0) printf("recoloring time in round %d: %f\n",i,maxRecoloringPerRound[i]);
        if(comm->getRank()==0) printf("min recoloring time in round %d: %f\n",i,minRecoloringPerRound[i]);
        if(comm->getRank()==0) printf("conflict detection time in round %d: %f\n",i,finalConflictDetectionPerRound[i]);
        if(comm->getRank()==0) printf("comm time in round %d: %f\n",i,finalCommPerRound[i]);
        if(comm->getRank()==0) printf("comp time in round %d: %f\n",i,finalCompPerRound[i]);
      }
      double global_total_time = 0.0;
      double global_recoloring_time=0.0;
      double global_min_recoloring_time=0.0;
      double global_conflict_detection=0.0;
      double global_comm_time=0.0;
      double global_comp_time=0.0;
      double global_interior_time = 0.0;
      Teuchos::reduceAll<int,double>(*comm, Teuchos::REDUCE_MAX,1,&total_time,&global_total_time);
      Teuchos::reduceAll<int,double>(*comm, Teuchos::REDUCE_MAX,1,&recoloring_time,&global_recoloring_time);
      Teuchos::reduceAll<int,double>(*comm, Teuchos::REDUCE_MIN,1,&recoloring_time,&global_min_recoloring_time);
      Teuchos::reduceAll<int,double>(*comm, Teuchos::REDUCE_MAX,1,&conflict_detection,&global_conflict_detection);
      Teuchos::reduceAll<int,double>(*comm, Teuchos::REDUCE_MAX,1,&comm_time,&global_comm_time);
      Teuchos::reduceAll<int,double>(*comm, Teuchos::REDUCE_MAX,1,&comp_time,&global_comp_time);
      Teuchos::reduceAll<int,double>(*comm, Teuchos::REDUCE_MAX,1,&interior_time,&global_interior_time);
      comm->barrier();
      fflush(stdout);
      if(comm->getRank()==0){
        printf("Boundary size: %d\n",totalBoundarySize);
        printf("Total Time: %f\n",global_total_time);
        printf("Interior Time: %f\n",global_interior_time);
        printf("Recoloring Time: %f\n",global_recoloring_time);
        printf("Min Recoloring Time: %f\n",global_min_recoloring_time);
        printf("Conflict Detection Time: %f\n",global_conflict_detection);
        printf("Comm Time: %f\n",global_comm_time);
        printf("Comp Time: %f\n",global_comp_time);
      }

    }
}; //end class

template <typename Adapter>
void AlgDistance2<Adapter>::buildModel(modelFlag_t &flags){
  flags.set(REMOVE_SELF_EDGES);
  
  this->env->debug(DETAILED_STATUS, "   building graph model");
  this->model = rcp(new GraphModel<base_adapter_t>(this->adapter, this->env,
                                                   this->comm, flags));
  this->env->debug(DETAILED_STATUS, "   graph model built");
}

template<typename Adapter>
void AlgDistance2<Adapter>::colorInterior(const size_t nVtx,
                       Kokkos::View<lno_t*,device_type> adjs_view,
                       Kokkos::View<offset_t*, device_type> offset_view,
                       Teuchos::RCP<femv_t> femv,
                       bool recolor) {
  using KernelHandle = KokkosKernels::Experimental::KokkosKernelsHandle
      <offset_t, lno_t, lno_t, execution_space, memory_space, memory_space>;
  
  KernelHandle kh;
  
  if(recolor)kh.create_distance2_graph_coloring_handle(KokkosGraph::COLORING_D2_VB);
  else kh.create_distance2_graph_coloring_handle(KokkosGraph::COLORING_D2_NB_BIT);
  //kh.get_distance2_graph_coloring_handle()->set_verbose(true);
  //kh.get_distance2_graph_coloring_handle()->set_tictoc(true);
  Kokkos::View<int**, Kokkos::LayoutLeft> femvColors = femv->template getLocalView<memory_space>();
  Kokkos::View<int*, device_type> sv = subview(femvColors, Kokkos::ALL, 0);
  kh.get_distance2_graph_coloring_handle()->set_vertex_colors(sv);

  KokkosGraph::Experimental::graph_color_distance2(&kh, nVtx, offset_view, adjs_view);
  numColors = kh.get_distance2_graph_coloring_handle()->get_num_colors();
  std::cout<<"\nKokkosKernels Coloring: "<<kh.get_distance2_graph_coloring_handle()->get_overall_coloring_time()<<"\n";
}//end colorInterior

}//end namespace Zoltan2

#endif

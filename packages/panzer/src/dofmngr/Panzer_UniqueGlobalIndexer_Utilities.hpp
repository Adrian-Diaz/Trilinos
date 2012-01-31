#ifndef __Panzer_UniqueGlobalIndexer_Utilities_decl_hpp__
#define __Panzer_UniqueGlobalIndexer_Utilities_decl_hpp__

#include <map>
#include <string>

#include <boost/unordered_map.hpp>

#include <Kokkos_DefaultNode.hpp>
#include <Tpetra_Vector.hpp>
#include <Tpetra_MultiVector.hpp>

#include "Panzer_UniqueGlobalIndexer.hpp"

namespace panzer {

/** Construct a vector that contains a reduced set of field numbers.
  * The ordering is based on the ordering from <code>ugi.getOwnedAndSharedIndices()</code>.
  * The term "reduced" means that this processor must be able to fully determine the
  * field number for each global number. There are some cases where processors are split
  * across element blocks with differing fields that this is nontrivial. 
  *
  * \param[in] ugi Global indexer to use.
  * 
  * \returns Reduced vector containing the field numbers.
  *
  * \note The description and use of this function are equally confusing...
  */
template <typename LocalOrdinalT,typename GlobalOrdinalT,typename Node>
Teuchos::RCP<Tpetra::Vector<int,std::size_t,GlobalOrdinalT,Node> >
buildGhostedFieldReducedVector(const UniqueGlobalIndexer<LocalOrdinalT,GlobalOrdinalT> & ugi);

template <typename LocalOrdinalT,typename GlobalOrdinalT>
Teuchos::RCP<Tpetra::Vector<int,std::size_t,GlobalOrdinalT,Kokkos::DefaultNode::DefaultNodeType> >
buildGhostedFieldReducedVector(const UniqueGlobalIndexer<LocalOrdinalT,GlobalOrdinalT> & ugi)
{ return buildGhostedFieldReducedVector<LocalOrdinalT,GlobalOrdinalT,Kokkos::DefaultNode::DefaultNodeType>(ugi); }

/** This function builds a vector that defines fields for each global unknown.
  * Notice that requires global communication and uses (underneath) the <code>Tpetra</code>
  * vector hence the required node type template parameter.
  *
  * This function returns a vector that serves as a map between Global indices ordered from
  * <code>ugi.getOwnedAndSharedIndices()</code> and the corresponding field number.
  *
  * \param[in] ugi Unique global indexer object that defines the ordering, global ids and field numbers.
  * \param[in] reducedVec Reduced field vector to use.  If none is passed it is compute by <code>buildGhostedFieldReducedVector</code>
  */
template <typename LocalOrdinalT,typename GlobalOrdinalT,typename Node>
Teuchos::RCP<const Tpetra::Vector<int,std::size_t,GlobalOrdinalT,Node> >
buildGhostedFieldVector(const UniqueGlobalIndexer<LocalOrdinalT,GlobalOrdinalT> & ugi,
                        const Teuchos::RCP<const Tpetra::Vector<int,std::size_t,GlobalOrdinalT,Node> > & reducedVec=Teuchos::null);

/** Convenience function default to the basic Kokkos node type.
  */
template <typename LocalOrdinalT,typename GlobalOrdinalT>
Teuchos::RCP<const Tpetra::Vector<int,std::size_t,GlobalOrdinalT,Kokkos::DefaultNode::DefaultNodeType> >
buildGhostedFieldVector(const UniqueGlobalIndexer<LocalOrdinalT,GlobalOrdinalT> & ugi,
                        const Teuchos::RCP<const Tpetra::Vector<int,std::size_t,GlobalOrdinalT,Kokkos::DefaultNode::DefaultNodeType> > & reducedVec=Teuchos::null)
{ return buildGhostedFieldVector<LocalOrdinalT,GlobalOrdinalT,Kokkos::DefaultNode::DefaultNodeType>(ugi,reducedVec); }

/** This function builds a vector that defines fields for each global unknown.
  * Notice that requires global communication and uses (underneath) the <code>Tpetra</code>
  * vector hence the required node type template parameter.
  *
  * This function returns a vector that serves as a map between Global indices ordered from
  * <code>ugi.getOwnedAndSharedIndices()</code> and the corresponding field number.
  *
  * \param[in] ugi Unique global indexer object that defines the ordering, global ids and field numbers.
  * \param[out] fieldNumbers Field numbers ordered to match up with a call to <code>ugi.getOwnedAndSharedIndices()</code>.
  *                          Meaning that <code>fieldNumbers.size()</code> is the same length as the vector
  *                          build by a call to <code>getOwnedAndSharedIndices()</code>.
  * \param[in] reducedVec Reduced field vector to use.  If none is passed it is compute by <code>buildGhostedFieldReducedVector</code>
  */
template <typename LocalOrdinalT,typename GlobalOrdinalT,typename Node>
void buildGhostedFieldVector(const UniqueGlobalIndexer<LocalOrdinalT,GlobalOrdinalT> & ugi,
                             std::vector<int> & fieldNumbers,
                             const Teuchos::RCP<const Tpetra::Vector<int,std::size_t,GlobalOrdinalT,Node> > & reducedVec=Teuchos::null);

/** Convenience function default to the basic Kokkos node type.
  */
template <typename LocalOrdinalT,typename GlobalOrdinalT>
void buildGhostedFieldVector(const UniqueGlobalIndexer<LocalOrdinalT,GlobalOrdinalT> & ugi,
                             std::vector<int> & fieldNumbers,
                             const Teuchos::RCP<const Tpetra::Vector<int,std::size_t,GlobalOrdinalT,Kokkos::DefaultNode::DefaultNodeType> > & reducedVec=Teuchos::null)
{ buildGhostedFieldVector<LocalOrdinalT,GlobalOrdinalT,Kokkos::DefaultNode::DefaultNodeType>(ugi,fieldNumbers,reducedVec); }

/** Build a reduced data vector using the reduced field vector. Here reduced is meant in the
  * exact same context as for the field vectors.
  *
  * \param[in] fieldName Name of field data should be ordered as
  * \param[in] ugi Unique global indexer object that defines the ordering, global ids and field numbers.
  * \param[in] reducedFieldVec A reduced vector containing the correctly ordered field numbers.
  *                            Likely computed by <code>buildGhostedFieldReducedVector</code>.
  * \param[in] data Data to put in vector
  * \param[out] A Tpetra vector containing the data in the reduced format.  This is
  *             now available for an import to construct the true ghosted data vector. This
  *             map must match the reducedFieldVec map.
  */
template <typename ScalarT,typename ArrayT,typename LocalOrdinalT,typename GlobalOrdinalT,typename Node>
void updateGhostedDataReducedVector(const std::string & fieldName,const std::string blockId,
                                    const UniqueGlobalIndexer<LocalOrdinalT,GlobalOrdinalT> & ugi,
                                    const ArrayT & data,
                                    Tpetra::MultiVector<ScalarT,std::size_t,GlobalOrdinalT,Node> & dataVector);

/** Construct a map that only uses a certain field.
 */
template <typename GlobalOrdinalT,typename Node>
Teuchos::RCP<const Tpetra::Map<std::size_t,GlobalOrdinalT,Node> >
getFieldMap(int fieldNum,const Tpetra::Vector<int,std::size_t,GlobalOrdinalT,Node> & fieldVector);


/** This class assists in mapping arrays of field data to field vectors.
  */
template <typename LocalOrdinalT,typename GlobalOrdinalT,typename Node>
class ArrayToFieldVector {
public:
   /** Construct information for the unique global indexer. Notice that this
     * requires global communication.
     */
   ArrayToFieldVector(const Teuchos::RCP<const UniqueGlobalIndexer<LocalOrdinalT,GlobalOrdinalT> > & ugi);

   /** Get a Tpetra vector containing the data ordered according to 
     * the ordering from <code>UGI::getOwnedAndSharedIndices</code>.
     *
     * \param[in] fieldName Name of field this data is from
     * \param[in] data Array of data
     *
     * \returns Returns a vector populated with the data. This vector
     *          is related to the <code>UGI::getOwnedAndSharedIndices</code>.
     */
   template <typename ScalarT,typename ArrayT>
   Teuchos::RCP<Tpetra::MultiVector<ScalarT,std::size_t,GlobalOrdinalT,Node> >
   getGhostedDataVector(const std::string & fieldName,const std::map<std::string,ArrayT> & data) const;

   /** Get a Tpetra vector containing the data ordered according to 
     * the ordering from <code>UGI::getOwnedIndices</code>.
     *
     * \param[in] fieldName Name of field this data is from
     * \param[in] data Array of data
     *
     * \returns Returns a vector populated with the data. This vector
     *          is related to the <code>UGI::getOwnedIndices</code>.
     */
   template <typename ScalarT,typename ArrayT>
   Teuchos::RCP<Tpetra::MultiVector<ScalarT,std::size_t,GlobalOrdinalT,Node> >
   getDataVector(const std::string & fieldName,const std::map<std::string,ArrayT> & data) const;

protected:
   typedef Tpetra::Vector<int,std::size_t,GlobalOrdinalT,Node> IntVector;
   typedef Tpetra::Map<std::size_t,GlobalOrdinalT,Node> Map;

   //! build unghosted field vector from ghosted field vector
   void buildFieldVector(const Tpetra::Vector<int,std::size_t,GlobalOrdinalT,Node> & source) const;

   //! DOF mapping
   Teuchos::RCP<const UniqueGlobalIndexer<LocalOrdinalT,GlobalOrdinalT> > ugi_;

   Teuchos::RCP<const IntVector> gh_reducedFieldVector_; //! ghosted reduced field vector
   Teuchos::RCP<const IntVector> gh_fieldVector_;        //! ghosted field vector

   mutable std::map<int,Teuchos::RCP<const Map> > gh_reducedFieldMaps_; //! Maps for each field (as needed)
   mutable std::map<int,Teuchos::RCP<const Map> > gh_fieldMaps_;        //! Maps for each field (as needed)

   mutable Teuchos::RCP<const IntVector> fieldVector_;               //! (unghosted) field vector (as needed)
   mutable std::map<int,Teuchos::RCP<const Map> > fieldMaps_;        //! Maps for each field (as needed)

private:
   // hide some constructors
   ArrayToFieldVector();
   ArrayToFieldVector(const ArrayToFieldVector &);
};


} // end namspace panzer

#include "Panzer_UniqueGlobalIndexer_Utilities_impl.hpp"

#endif

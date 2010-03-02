/*------------------------------------------------------------------------*/
/*                 Copyright 2010 Sandia Corporation.                     */
/*  Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive   */
/*  license for use of this work by or on behalf of the U.S. Government.  */
/*  Export of this program may require a license from the                 */
/*  United States Government.                                             */
/*------------------------------------------------------------------------*/


#include <stk_mesh/base/FieldData.hpp>
#include <unit_tests/UnitTestBoxMeshFixture.hpp>


BoxMeshFixture::~BoxMeshFixture()
{
  delete m_bulk_data ;
  delete m_meta_data ;
}

BoxMeshFixture::BoxMeshFixture( stk::ParallelMachine pm )
{
  typedef shards::Hexahedron<8> Hex8 ;
  enum { SpatialDim = 3 };
  enum { NodesPerElem = Hex8::node_count };


  m_meta_data = new stk::mesh::MetaData( stk::mesh::fem_entity_type_names() );

  m_coord_field = &m_meta_data->declare_field<CoordFieldType>("Coordinates");
  m_coord_gather_field = &m_meta_data->declare_field<CoordGatherFieldType>("GatherCoordinates");

  //put coord-field on all nodes:
  stk::mesh::put_field(*m_coord_field, stk::mesh::Node, m_meta_data->universal_part(), SpatialDim );

  //put coord-gather-field on all elements:
  stk::mesh::put_field(*m_coord_gather_field, stk::mesh::Element, m_meta_data->universal_part(), NodesPerElem);

  m_meta_data->declare_field_relation(*m_coord_gather_field, stk::mesh::element_node_stencil<shards::Hexahedron<8> >, *m_coord_field);

  stk::mesh::Part& elem_block = m_meta_data->declare_part("block1", stk::mesh::Element);
  stk::mesh::set_cell_topology<shards::Hexahedron<8> >(elem_block);

  m_meta_data->commit();

  m_bulk_data = new stk::mesh::BulkData(*m_meta_data, pm);

  const unsigned p_size = stk::parallel_machine_size( pm );
  const unsigned p_rank = stk::parallel_machine_rank( pm );
  const unsigned num_elems = 8 ;

  for ( int iz = 0 ; iz < 3 ; ++iz ) {
  for ( int iy = 0 ; iy < 3 ; ++iy ) {
  for ( int ix = 0 ; ix < 3 ; ++ix ) {
    m_node_id[iz][iy][ix] = 1 + ix + 3 * ( iy + 3 * iz );
    m_node_coord[iz][iy][ix][0] = ix ;
    m_node_coord[iz][iy][ix][1] = iy ;
    m_node_coord[iz][iy][ix][2] = - iz ;
  }
  }
  }

  const unsigned beg_elem = ( num_elems * p_rank ) / p_size ;
  const unsigned end_elem = ( num_elems * ( p_rank + 1 ) ) / p_size ;

  unsigned elem = 0 ;
  for ( int iz = 0 ; iz < 2 ; ++iz ) {
  for ( int iy = 0 ; iy < 2 ; ++iy ) {
  for ( int ix = 0 ; ix < 2 ; ++ix , ++elem ) {
    if ( beg_elem <= elem && elem < end_elem ) {
      stk::mesh::EntityId elem_id = 1 + ix + 3 * ( iy + 3 * iz );
      stk::mesh::EntityId elem_node[8] ;
      Scalar * node_coord[8] ;

      elem_node[0] = m_node_id[iz  ][iy  ][ix  ] ;
      elem_node[1] = m_node_id[iz  ][iy  ][ix+1] ;
      elem_node[2] = m_node_id[iz+1][iy  ][ix+1] ;
      elem_node[3] = m_node_id[iz+1][iy  ][ix  ] ;
      elem_node[4] = m_node_id[iz  ][iy+1][ix  ] ;
      elem_node[5] = m_node_id[iz  ][iy+1][ix+1] ;
      elem_node[6] = m_node_id[iz+1][iy+1][ix+1] ;
      elem_node[7] = m_node_id[iz+1][iy+1][ix  ] ;

      node_coord[0] = m_node_coord[iz  ][iy  ][ix  ] ;
      node_coord[1] = m_node_coord[iz  ][iy  ][ix+1] ;
      node_coord[2] = m_node_coord[iz+1][iy  ][ix+1] ;
      node_coord[3] = m_node_coord[iz+1][iy  ][ix  ] ;
      node_coord[4] = m_node_coord[iz  ][iy+1][ix  ] ;
      node_coord[5] = m_node_coord[iz  ][iy+1][ix+1] ;
      node_coord[6] = m_node_coord[iz+1][iy+1][ix+1] ;
      node_coord[7] = m_node_coord[iz+1][iy+1][ix  ] ;

      stk::mesh::Entity& element = stk::mesh::declare_element(*m_bulk_data, elem_block, elem_id, elem_node);

      stk::mesh::PairIterRelation rel = element.relations(stk::mesh::Node);

      for(int i=0; i< 8 ; ++i ) {
        stk::mesh::Entity& node = *rel[i].entity();
        Scalar* data = stk::mesh::field_data(*m_coord_field, node);
        data[0] = node_coord[i][0];
        data[1] = node_coord[i][1];
        data[2] = node_coord[i][2];
      }
    }
  }
  }
  }

  m_bulk_data->modification_end();

  for ( int iz = 0 ; iz < 3 ; ++iz ) {
  for ( int iy = 0 ; iy < 3 ; ++iy ) {
  for ( int ix = 0 ; ix < 3 ; ++ix ) {
    // Find node
    stk::mesh::EntityId node_id = 1 + ix + 3 * ( iy + 3 * iz );
    m_nodes[iz][iy][ix] = m_bulk_data->get_entity( stk::mesh::Node , node_id );
  }
  }
  }

  for ( int iz = 0 ; iz < 2 ; ++iz ) {
  for ( int iy = 0 ; iy < 2 ; ++iy ) {
  for ( int ix = 0 ; ix < 2 ; ++ix , ++elem ) {
    stk::mesh::EntityId elem_id = 1 + ix + 3 * ( iy + 3 * iz );
    // Find element
    m_elems[iz][iy][ix] = m_bulk_data->get_entity( stk::mesh::Element , elem_id );
  }
  }
  }
}


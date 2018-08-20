//----------------
// Includes
//----------------

#include "MantidGeometry/Objects/IObject.h"
#include "MantidGeometry/Objects/MeshObject.h"
#include "MantidGeometry/Objects/MeshObject2D.h"
#include "MantidKernel/V3D.h"
#include "MantidNexusGeometry/NexusShapeFactory.h"
#include <cxxtest/TestSuite.h>

using namespace Mantid::NexusGeometry::NexusShapeFactory;
using Mantid::Kernel::V3D;

class NexusShapeFactoryTest : public CxxTest::TestSuite {
public:
  void test_make_2D_mesh() {

    std::vector<V3D> vertices;
    vertices.emplace_back(V3D(-1, 0, 0));
    vertices.emplace_back(V3D(1, 0, 0));
    vertices.emplace_back(V3D(0, 1, 0));
    std::vector<uint16_t> triangles;
    triangles.insert(triangles.end(), {0, 1, 2});

    auto obj = createMesh(std::move(triangles), std::move(vertices));
    auto mesh2d =
        dynamic_cast<const Mantid::Geometry::MeshObject2D *>(obj.get());
    TS_ASSERT(mesh2d); // Check right dynamic type
    TS_ASSERT_EQUALS(mesh2d->numberOfTriangles(), 1); // 3 vertices (1 triangle)
  }

  void test_make_3D_mesh() {

    std::vector<V3D> vertices;
    vertices.emplace_back(V3D(-1, 0, 0));
    vertices.emplace_back(V3D(1, 0, 0));
    vertices.emplace_back(V3D(0, 1, 0));
    vertices.emplace_back(V3D(0, 1, 1));
    std::vector<uint16_t> triangles;
    triangles.insert(triangles.end(), {0, 1, 2, 1, 3, 2, 3, 0, 2});

    auto obj = createMesh(std::move(triangles), std::move(vertices));
    auto mesh = dynamic_cast<const Mantid::Geometry::MeshObject *>(obj.get());
    TS_ASSERT(mesh);                                // Check right dynamic type
    TS_ASSERT_EQUALS(mesh->numberOfTriangles(), 3); // 4 vertices (3 triangles)
  }
};

class NexusShapeFactoryTestPerformance : public CxxTest::TestSuite {
private:
  std::vector<Eigen::Vector3d> m_vertices;
  std::vector<uint16_t> m_facesIndices;
  std::vector<uint16_t> m_windingOrder;

public:
  // This pair of boilerplate methods prevent the suite being created statically
  // This means the constructor isn't called when running other tests
  static NexusShapeFactoryTestPerformance *createSuite() {
    return new NexusShapeFactoryTestPerformance();
  }
  static void destroySuite(NexusShapeFactoryTestPerformance *suite) {
    delete suite;
  }

  NexusShapeFactoryTestPerformance() {
    // Make inputs. Repeated squares
    for (uint16_t i = 0; i < 10000; ++i) {
      m_vertices.emplace_back(Eigen::Vector3d(0 + i, 1, 0));
      m_vertices.emplace_back(Eigen::Vector3d(0 + i, 1, 0));
      /*
       *     x           x     x
       *     |           |     |
       *     |      ->   |     |
       *     x           x     x
       */

      m_facesIndices.push_back(i * 4);
      m_facesIndices.push_back((i + 1) * 4);
      if (i % 2 != 0) {
        m_windingOrder.push_back(i * 2);
        m_windingOrder.push_back(i * 2 + 1);
        m_windingOrder.push_back(i * 2 + 2);
        m_windingOrder.push_back(i * 2 + 3);
      }
    }
  }

  void test_createFromOFFMesh() {
    auto mesh = createFromOFFMesh(m_facesIndices, m_windingOrder, m_vertices);
    TS_ASSERT(mesh.get() != nullptr)
  }
};

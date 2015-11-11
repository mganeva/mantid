#ifndef MANTID_RULESCOMPGRPTEST__
#define MANTID_RULESCOMPGRPTEST__
#include <cxxtest/TestSuite.h>
#include <cmath>
#include <vector>
#include "MantidKernel/Logger.h"
#include "MantidKernel/System.h"
#include <cfloat>

#include "boost/make_shared.hpp"
#include "MantidKernel/make_unique.h"
#include "MantidKernel/V3D.h"
#include "MantidGeometry/Surfaces/Quadratic.h"
#include "MantidGeometry/Objects/Object.h"
#include "MantidGeometry/Objects/Rules.h"
#include "MantidGeometry/Surfaces/Plane.h"
#include "MantidGeometry/Surfaces/Sphere.h"
#include "MantidGeometry/Surfaces/Cylinder.h"
#include "MantidGeometry/Surfaces/Cone.h"

using namespace Mantid;
using namespace Geometry;
using Mantid::Kernel::V3D;

class RulesCompGrpTest : public CxxTest::TestSuite {
public:
  void testConstructor() {
    CompGrp A;
    TS_ASSERT_EQUALS(A.display(), "");
    TS_ASSERT_EQUALS(A.leaf(0), (Rule *)0);
    TS_ASSERT_EQUALS(A.isComplementary(), 1);
  }

  void testTwoRuleConstructor() {
    Intersection Parent;
    auto uSC = createUnionSphereAndCylinder();
    CompGrp A(&Parent, std::move(uSC));
    TS_ASSERT_EQUALS(A.getParent(), &Parent);
    // TS_ASSERT_EQUALS(A.leaf(0), uSC);
    TS_ASSERT_EQUALS(A.isComplementary(), 1);
    TS_ASSERT_EQUALS(A.display(), "#( -10 : -11 )");
  }

  void testCompGrpConstructor() {
    CompGrp A;
    auto uSC = createUnionSphereAndCylinder();
    A.setLeaf(std::move(uSC), 0);
    // TS_ASSERT_EQUALS(A.leaf(0), uSC);
  }

  void testClone() {
    CompGrp A;
    auto uSC = createUnionSphereAndCylinder();
    A.setLeaf(std::move(uSC), 0);
    // TS_ASSERT_EQUALS(A.leaf(0), uSC);
    auto B = A.clone();
    // TS_ASSERT_EQUALS(B->leaf(0)->display(), uSC->display());
  }

  void testAssignment() {
    CompGrp A;
    auto uSC = createUnionSphereAndCylinder();
    A.setLeaf(std::move(uSC), 0);
    // TS_ASSERT_EQUALS(A.leaf(0), uSC);
  }

  void testSetLeaves() {
    CompGrp A;
    auto uSC = createUnionSphereAndCylinder();
    A.setLeaves(std::move(uSC), std::unique_ptr<Rule>());
    // TS_ASSERT_EQUALS(A.leaf(0), uSC);
    TS_ASSERT_EQUALS(A.display(), "#( -10 : -11 )");
  }

  void testFindLeaf() {
    CompGrp A, B;
    auto uSC = createUnionSphereAndCylinder();
    Rule *ptruSC = uSC.get();
    A.setLeaf(std::move(uSC), 0);
    TS_ASSERT_EQUALS(A.leaf(0), ptruSC);
    TS_ASSERT_EQUALS(A.findLeaf(ptruSC), 0);
    TS_ASSERT_EQUALS(A.findLeaf(&B), -1);
  }

  void testFindKey() {
    CompGrp A;
    auto uSC = createUnionSphereAndCylinder();
    A.setLeaf(std::move(uSC), 0);
    // TS_ASSERT_EQUALS(A.leaf(0), uSC);
    TS_ASSERT_EQUALS(A.findKey(0), (Rule *)0); // Always returns 0
  }

  void testIsValid() {
    CompGrp A;
    auto uSC = createUnionSphereAndCylinder();
    A.setLeaf(std::move(uSC), 0);
    // TS_ASSERT_EQUALS(A.leaf(0), uSC);
    TS_ASSERT_EQUALS(A.isValid(V3D(0.0, 0.0, 0.0)),
                     false); // inside the sphere and cylinder
    TS_ASSERT_EQUALS(A.isValid(V3D(4.1, 0.0, 0.0)), true);  // outside sphere
    TS_ASSERT_EQUALS(A.isValid(V3D(4.0, 0.0, 0.0)), false); // on sphere
    TS_ASSERT_EQUALS(A.isValid(V3D(3.9, 0.0, 0.0)), false); // inside sphere
    TS_ASSERT_EQUALS(A.isValid(V3D(1.1, 4.0, 0.0)), true);  // outside cylinder
    TS_ASSERT_EQUALS(A.isValid(V3D(1.0, 4.0, 0.0)), false); // on cylinder
    TS_ASSERT_EQUALS(A.isValid(V3D(0.9, 4.0, 0.0)), false); // inside cylinder
  }

  void testIsValidMap() {
    CompGrp A;
    auto uSC = createUnionSphereAndCylinder();
    A.setLeaf(std::move(uSC), 0);
    // TS_ASSERT_EQUALS(A.leaf(0), uSC);
    std::map<int, int> input;
    input[10] = 1;
    input[11] = 1;
    TS_ASSERT_EQUALS(A.isValid(input), true);
    input[10] = 0;
    TS_ASSERT_EQUALS(A.isValid(input), false);
    input[11] = 0;
    TS_ASSERT_EQUALS(A.isValid(input), false);
    input[10] = 1;
    TS_ASSERT_EQUALS(A.isValid(input), false);
  }

  void testSimplyfy() {
    CompGrp A;
    auto uSC = createUnionSphereAndCylinder();
    A.setLeaf(std::move(uSC), 0);
    // TS_ASSERT_EQUALS(A.leaf(0), uSC);
    TS_ASSERT_EQUALS(
        A.simplify(),
        0); // Always return 0 bcos a single node cannot be simplified
  }

private:
  std::unique_ptr<Rule> createUnionSphereAndCylinder() {
    auto sR1 = Mantid::Kernel::make_unique<SurfPoint>();
    auto sR2 = Mantid::Kernel::make_unique<SurfPoint>();
    auto sP = boost::make_shared<Sphere>();
    sP->setSurface("s 2.0 0.0 0.0 2");
    sR1->setKey(sP); // Sphere
    sR1->setKeyN(-10);
    auto cP = boost::make_shared<Cylinder>();
    cP->setSurface("cy 1.0");
    sR2->setKey(cP); // cappedcylinder
    sR2->setKeyN(-11);
    return Mantid::Kernel::make_unique<Union>(std::move(sR1), std::move(sR2));
  }
};
//---------------------------------End of
// CompGrp----------------------------------------

#endif

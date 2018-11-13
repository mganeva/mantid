// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2018 ISIS Rutherford Appleton Laboratory UKRI,
//     NScD Oak Ridge National Laboratory, European Spallation Source
//     & Institut Laue - Langevin
// SPDX - License - Identifier: GPL - 3.0 +
#ifndef MANTID_ALGORITHMS_SAMPLELOGSBEHAVIOURTEST_H_
#define MANTID_ALGORITHMS_SAMPLELOGSBEHAVIOURTEST_H_

#include "MantidAPI/MatrixWorkspace.h"
#include "MantidAlgorithms/RunCombinationHelpers/SampleLogsBehaviour.h"
#include "MantidDataHandling/LoadParameterFile.h"
#include "MantidTestHelpers/WorkspaceCreationHelper.h"
#include <cxxtest/TestSuite.h>

#include "MantidKernel/TimeSeriesProperty.h"

using Mantid::Algorithms::SampleLogsBehaviour;
using namespace Mantid::API;
using namespace Mantid::DataHandling;
using namespace Mantid::Kernel;
using namespace WorkspaceCreationHelper;

class SampleLogsBehaviourTest : public CxxTest::TestSuite {
public:
  // This pair of boilerplate methods prevent the suite being created statically
  // This means the constructor isn't called when running other tests
  static SampleLogsBehaviourTest *createSuite() {
    return new SampleLogsBehaviourTest();
  }
  static void destroySuite(SampleLogsBehaviourTest *suite) { delete suite; }

  // Please note that many tests are currently present in MergeRunsTest.

  void testMergeRunsIPFNames() {
    // Using default values of the constructor
    Logger log("testLog");
    auto ws = createTestingWorkspace();
    SampleLogsBehaviour sbh = SampleLogsBehaviour(ws, log);
    TS_ASSERT_THROWS_NOTHING(sbh.mergeSampleLogs(ws, ws));
    const std::string A = ws->run().getLogData("A")->value();
    const std::string B = ws->run().getLogData("B")->value();
    const std::string C = ws->run().getLogData("C")->value();
    // A listed and B summed according to IPF
    TS_ASSERT_EQUALS(A, "2.6499999999999999, 2.6499999999999999")
    TS_ASSERT_EQUALS(B, "3.1200000000000001")
    TS_ASSERT_EQUALS(C, "8.5500000000000007")
    testUnitsTestWorkspace(ws);
  }

  void testMergeRunsUserNames() {
    // Using default values of the constructor
    Logger log("testLog");
    auto ws = createTestingWorkspace();
    SampleLogsBehaviour::SampleLogNames sampleLogNames;
    sampleLogNames.sampleLogsSum = "A";
    SampleLogsBehaviour sbh = SampleLogsBehaviour(ws, log, sampleLogNames);
    TS_ASSERT_THROWS_NOTHING(sbh.mergeSampleLogs(ws, ws));
    const std::string A = ws->run().getLogData("A")->value();
    const std::string B = ws->run().getLogData("B")->value();
    const std::string C = ws->run().getLogData("C")->value();
    // A summed according to user name and B summed according to IPF
    TS_ASSERT_EQUALS(A, "5.2999999999999998")
    TS_ASSERT_EQUALS(B, "3.1200000000000001")
    TS_ASSERT_EQUALS(C, "8.5500000000000007")
    testUnitsTestWorkspace(ws);
  }

  void testConjoinXRunsIPFNames() {
    // Using prefix conjoin_ + default value names for constructing
    Logger log("testLog");
    auto ws = createTestingWorkspace();
    SampleLogsBehaviour::SampleLogNames sampleLogNames;
    SampleLogsBehaviour::ParameterName parameterNames;
    parameterNames.SUM_MERGE = "conjoin_sample_logs_sum";
    SampleLogsBehaviour sbh =
        SampleLogsBehaviour(ws, log, sampleLogNames, parameterNames);
    sbh.mergeSampleLogs(ws, ws);
    const std::string A = ws->run().getLogData("A")->value();
    const std::string B = ws->run().getLogData("B")->value();
    const std::string C = ws->run().getLogData("C")->value();
    // A and C summed according to IPF
    TS_ASSERT_EQUALS(A, "5.2999999999999998")
    TS_ASSERT_EQUALS(B, "1.5600000000000001")
    TS_ASSERT_EQUALS(C, "17.100000000000001")
    testUnitsTestWorkspace(ws);
  }

  void testConjoinXRunsUserNames() {
    // Using prefix conjoin_ + default value names for constructing
    Logger log("testLog");
    auto ws = createTestingWorkspace();
    SampleLogsBehaviour::SampleLogNames sampleLogNames;
    sampleLogNames.sampleLogsSum = "B";
    SampleLogsBehaviour::ParameterName parameterNames;
    parameterNames.SUM_MERGE = "conjoin_sample_logs_sum";
    SampleLogsBehaviour sbh =
        SampleLogsBehaviour(ws, log, sampleLogNames, parameterNames);
    sbh.mergeSampleLogs(ws, ws);
    const std::string A = ws->run().getLogData("A")->value();
    const std::string B = ws->run().getLogData("B")->value();
    const std::string C = ws->run().getLogData("C")->value();
    // B summed according to user name and A and C summed according to IPF
    TS_ASSERT_EQUALS(A, "5.2999999999999998")
    TS_ASSERT_EQUALS(B, "3.1200000000000001")
    TS_ASSERT_EQUALS(C, "17.100000000000001")
    testUnitsTestWorkspace(ws);
  }

  void test_sum_unit() {
    // Using default values of the constructor
    Logger log("testLog");
    auto ws1 = createTestingWorkspace();
    auto ws2 = createTestingWorkspace();
    SampleLogsBehaviour::SampleLogNames sampleLogNames;
    sampleLogNames.sampleLogsSum = "A";
    SampleLogsBehaviour sbh = SampleLogsBehaviour(ws1, log, sampleLogNames);
    TS_ASSERT_THROWS_NOTHING(sbh.mergeSampleLogs(ws1, ws2));
    // All units must not have changed:
    testUnitsTestWorkspace(ws1);
  }

  void test_list_unit() {
    // Using default values of the constructor
    Logger log("testLog");
    auto ws1 = createTestingWorkspace();
    auto ws2 = createTestingWorkspace();
    SampleLogsBehaviour::SampleLogNames sampleLogNames;
    sampleLogNames.sampleLogsList = "A";
    SampleLogsBehaviour sbh = SampleLogsBehaviour(ws1, log, sampleLogNames);
    TS_ASSERT_THROWS_NOTHING(sbh.mergeSampleLogs(ws1, ws2));
    // All units must not have changed:
    testUnitsTestWorkspace(ws1);
  }

  void test_time_series_unit() {
    // Using default values of the constructor
    Logger log("testLog");
    auto ws1 = createTestingWorkspace();
    auto ws2 = createTestingWorkspace();
    SampleLogsBehaviour::SampleLogNames sampleLogNames;
    sampleLogNames.sampleLogsTimeSeries = "D";
    SampleLogsBehaviour sbh = SampleLogsBehaviour(ws1, log, sampleLogNames);
    TS_ASSERT_THROWS_NOTHING(sbh.mergeSampleLogs(ws1, ws2));
    // All units must not have changed:
    testUnitsTestWorkspace(ws1);
  }

  MatrixWorkspace_sptr createTestingWorkspace() {
    MatrixWorkspace_sptr ws = create2DWorkspaceWithFullInstrument(
        3, 3, true, false, true, m_instrName);
    // Add sample logs
    TS_ASSERT_THROWS_NOTHING(
        ws->mutableRun().addLogData(new PropertyWithValue<double>("A", 2.65)))
    TS_ASSERT_THROWS_NOTHING(
        ws->mutableRun().addLogData(new PropertyWithValue<double>("B", 1.56)))
    TS_ASSERT_THROWS_NOTHING(
        ws->mutableRun().addLogData(new PropertyWithValue<double>("C", 8.55)))
    TimeSeriesProperty<double> *time_series_log =
        new TimeSeriesProperty<double>("D");
    TS_ASSERT_THROWS_NOTHING(
        time_series_log->addValue("2018-11-30T16:17:01", 5.5))
    TS_ASSERT_THROWS_NOTHING(
        time_series_log->addValue("2018-11-30T16:17:02", 6.6))
    TS_ASSERT_THROWS_NOTHING(
        time_series_log->addValue("2018-11-30T16:17:03", 7.7))
    TS_ASSERT_THROWS_NOTHING(ws->mutableRun().addProperty(time_series_log))
    // Add units to the sample logs
    TS_ASSERT_THROWS_NOTHING(ws->getLog("A")->setUnits("A_unit"))
    TS_ASSERT_THROWS_NOTHING(ws->getLog("B")->setUnits("B_unit"))
    TS_ASSERT_THROWS_NOTHING(ws->getLog("C")->setUnits("C_unit"))
    TS_ASSERT_THROWS_NOTHING(ws->getLog("D")->setUnits("D_unit"))
    testUnitsTestWorkspace(ws);
    // Load test parameter file
    LoadParameterFile addIPF;
    TS_ASSERT_THROWS_NOTHING(addIPF.initialize());
    TS_ASSERT_THROWS_NOTHING(addIPF.setProperty("ParameterXML", m_parameterXML))
    TS_ASSERT_THROWS_NOTHING(addIPF.setProperty("Workspace", ws))
    TS_ASSERT_THROWS_NOTHING(addIPF.execute())
    TS_ASSERT(addIPF.isExecuted())
    return ws;
  }

  void testUnitsTestWorkspace(MatrixWorkspace_sptr ws) {
    // All units must not have changed:
    TS_ASSERT_EQUALS(ws->getLog("A")->units(), "A_unit")
    TS_ASSERT_EQUALS(ws->getLog("B")->units(), "B_unit")
    TS_ASSERT_EQUALS(ws->getLog("C")->units(), "C_unit")
    TS_ASSERT_EQUALS(ws->getLog("D")->units(), "D_unit")
  }

private:
  // Test instrument name
  const std::string m_instrName = "INSTR";
  // Define parameter XML string
  std::string m_parameterXML =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>"
      "<parameter-file instrument=\"INSTR\" valid-from=\"2018-11-07 "
      "12:00:00\">"
      "  <component-link name=\"INSTR\">"
      "    <!-- For MergeRuns.-->"
      "    <parameter name=\"sample_logs_sum\" type=\"string\">"
      "	      <value val=\"B\" />"
      "    </parameter>"
      "    <parameter name=\"sample_logs_list\" type=\"string\">"
      "	      <value val=\"A\" />"
      "    </parameter>"
      "    <parameter name=\"sample_logs_time_series\" type=\"string\">"
      "	      <value val=\"D\" />"
      "    </parameter>"
      "    <!-- For ConjoinXRuns. -->"
      "    <parameter name=\"conjoin_sample_logs_sum\" type=\"string\">"
      "       <value val=\"A, C\" />"
      "    </parameter>"
      "  </component-link>"
      "</parameter-file>";
};

#endif /* MANTID_ALGORITHMS_SAMPLELOGSBEHAVIOURTEST_H_ */

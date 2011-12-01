import unittest
import sys

from mantid.api import algorithm_factory, analysis_data_svc

from mantid import simpleapi

class SimpleAPITest(unittest.TestCase):

    def tearDown(self):
        analysis_data_svc.clear()
    
    def test_module_dict_seems_to_be_correct_size(self):
        # Check that the module has at least the same number
        # of attributes as unique algorithms
        module_dict = dir(simpleapi)
        all_algs = algorithm_factory.get_registered_algorithms(True)
        self.assertTrue( len(module_dict) > len(all_algs) )
        
    def test_alg_has_expected_doc_string(self):
        # Test auto generated string, Load is manually written
        expected_doc = \
        """Rebins data with new X bin boundaries. For EventWorkspaces, you can very quickly rebin in-place by keeping the same output name and PreserveEvents=true.

Property descriptions: 

InputWorkspace(Input:req) *MatrixWorkspace*       Workspace containing the input data

OutputWorkspace(Output:req) *MatrixWorkspace*       The name to give the output workspace

Params(Input:req) *dbl list*       A comma separated list of first bin boundary, width, last bin boundary. Optionally
this can be followed by a comma and more widths and last boundary pairs.
Negative width values indicate logarithmic binning.

PreserveEvents(Input) *boolean*       Keep the output workspace as an EventWorkspace, if the input has events (default).
If the input and output EventWorkspace names are the same, only the X bins are set, which is very quick.
If false, then the workspace gets converted to a Workspace2D histogram.
"""
        doc = simpleapi.rebin.__doc__
        self.assertTrue(len(doc) > 0 )
        self.assertEquals(doc, expected_doc)
        
    def test_function_call_executes_correct_algorithm_when_passed_correct_args(self):
        wsname = 'test_function_call_executes_correct_algorithm_when_passed_correct_args'
        data = [1.0,2.0,3.0,4.0,5.0]
        simpleapi.CreateWorkspace(data,data,OutputWorkspace=wsname,NSpec=1,UnitX='Wavelength')
        self.assertTrue( wsname in analysis_data_svc )
        
    def test_function_call_executes_with_output_workspace_on_lhs(self):
        data = [1.0,2.0,3.0,4.0,5.0]
        wavelength = simpleapi.CreateWorkspace(data,data,NSpec=1,UnitX='Wavelength')
        wsname = 'wavelength'
        self.assertTrue( wsname in analysis_data_svc )
        
    def test_function_call_executes_when_algorithm_has_only_inout_workspace_props(self):
        data = [1.0,2.0,3.0,4.0,5.0, 6.0]
        wavelength = simpleapi.CreateWorkspace(data,data,NSpec=3,UnitX='Wavelength')
        simpleapi.MaskDetectors(wavelength,WorkspaceIndexList=[1,2])
        
    def test_function_call_raises_ValueError_when_passed_args_with_invalid_values(self):
        # lhs code bug means we can't do this "self.assertRaises(simpleapi.LoadNexus, 'DoesNotExist')" --> ticket #4186
        try:
            simpleapi.LoadNexus(Filename='DoesNotExist.nxs')
            self.fail("A ValueError was not thrown")
        except ValueError:
            pass
        
    def test_function_call_raises_RuntimeError_when_passed_incorrect_args(self):
        try:
            simpleapi.LoadNexus(NotAProperty=1)
            self.fail("A RuntimeError was not thrown")
        except RuntimeError:
            pass

    def test_function_call_raises_RuntimeError_if_num_of_ret_vals_doesnt_match_num_assigned_vars(self):
        try:
            ws, ws2 = simpleapi.CreateWorkspace([1.5],[1.5],NSpec=1,UnitX='Wavelength')
        except RuntimeError, exc:
            # Check the error is correct and it's not some random runtime error
            if 'CreateWorkspace is trying to return 1 output(s) but you have provided 2 variable(s). These numbers must match.' == str(exc):
                pass
            else:
                self.fail("Exception was raised but it did not have the correct message: '%s'" % str(exc))
        
    def _do_exec_time_props_test(self, runner):
        try:
            data, monitors = runner('IRS21360.raw', LoadMonitors='Separate')
        except Exception, exc:
            self.fail("An error occurred when returning outputs declared at algorithm execution: '%s'" % str(exc))
        
    def test_function_returns_correct_args_when_extra_output_props_are_added_at_execute_time(self):
        self._do_exec_time_props_test(simpleapi.LoadRaw)
        
    def test_Load_returns_correct_args_when_extra_output_props_are_added_at_execute_time(self):
        self._do_exec_time_props_test(simpleapi.Load)
    
    def test_Load_call_with_just_filename_executes_correctly(self):
        try:
            raw = simpleapi.Load('IRS21360.raw')
        except Runtime:
            self.fail("Load with a filename should not raise an exception")
        self.assertEquals(116, raw.get_number_histograms())
        analysis_data_svc.remove('raw')

    def test_Load_call_with_other_args_executes_correctly(self):
        try:
            raw = simpleapi.Load('IRS21360.raw',SpectrumMax=1)
        except Runtime:
            self.fail("Load with a filename and extra args should not raise an exception")
        self.assertEquals(1, raw.get_number_histograms())

    def test_Load_call_with_args_that_do_not_apply_executes_correctly(self):
        try:
            raw = simpleapi.Load('IRS21360.raw',SpectrumMax=1,Append=True)
        except Runtime:
            self.fail("Load with a filename and extra args should not raise an exception")
        self.assertEquals(1, raw.get_number_histograms())
    
    def test_that_dialog_call_raises_runtime_error(self):
        try:
            simpleapi.LoadEventNexusDialog()
        except RuntimeError, exc:
            msg = str(exc)
            if msg != "Can only display properties dialog in gui mode":
                self.fail("Dialog function raised the correct exception type but the message was wrong")
                

if __name__ == '__main__':
    unittest.main()
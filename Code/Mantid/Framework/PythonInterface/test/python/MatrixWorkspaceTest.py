import unittest,sys

from testhelpers import run_algorithm, can_be_instantiated

from mantid.api import MatrixWorkspace, WorkspaceProperty_Workspace, Workspace

import numpy as np

class MatrixWorkspaceTest(unittest.TestCase):

    _test_ws_prop = None
    _test_ws = None

    def setUp(self):
        if self._test_ws is None:
            wsname = 'LOQ48127'
            alg = run_algorithm('Load', Filename='LOQ48127.raw', OutputWorkspace=wsname, SpectrumMax=2, child=True)
            self.__class__._test_ws_prop = alg.get_property('OutputWorkspace')
            self.__class__._test_ws = alg.get_property('OutputWorkspace').value

    def test_that_one_cannot_be_instantiated_directly(self):
        self.assertFalse(can_be_instantiated(MatrixWorkspace))

    def test_meta_information(self):
        self.assertEquals(self._test_ws.id(), "Workspace2D")
        self.assertEquals(self._test_ws.name(), "")
        self.assertEquals(self._test_ws.get_title().rstrip(), "direct beam") #The title seems to have a newline in it
        self.assertEquals(self._test_ws.get_comment(), "")
        self.assertEquals(self._test_ws.is_dirty(), False)
        self.assertTrue(self._test_ws.get_memory_size() > 0.0)
        self.assertEquals(self._test_ws.thread_safe(), True)
        
    def test_workspace_data_information(self):
        self.assertEquals(self._test_ws.get_number_histograms(), 2)
        self.assertEquals(self._test_ws.blocksize(), 102)

    def test_that_a_histogram_workspace_is_returned_as_a_MatrixWorkspace_from_a_property(self):
        self.assertEquals(type(self._test_ws_prop), WorkspaceProperty_Workspace)
        # Is Workspace in the hierarchy of the value
        self.assertTrue(isinstance(self._test_ws, Workspace))
        # Have got a MatrixWorkspace back and not just the generic interface
        self.assertEquals(type(self._test_ws), MatrixWorkspace)
        mem = self._test_ws.get_memory_size()
        self.assertTrue( (mem > 0) )

    def test_data_can_be_extracted_to_numpy_successfully(self):
        x = self._test_ws.extract_x()
        y = self._test_ws.extract_y()
        e = self._test_ws.extract_e()

        self._do_numpy_comparison(self._test_ws, x, y, e)

    def _do_numpy_comparison(self, workspace, x_np, y_np, e_np):
        nhist = workspace.get_number_histograms()
        blocksize = workspace.blocksize()
        for arr in (x_np, y_np, e_np):
            self.assertEquals(type(arr), np.ndarray)
        
        self.assertEquals(x_np.shape, (nhist, blocksize + 1)) # 2 rows, 103 columns
        self.assertEquals(y_np.shape, (nhist, blocksize)) # 2 rows, 102 columns
        self.assertEquals(e_np.shape, (nhist, blocksize)) # 2 rows, 102 columns

        
        for i in range(nhist):
            for j in range(blocksize):
                self.assertEquals(x_np[i][j], workspace.read_x(i)[j])
                self.assertEquals(y_np[i][j], workspace.read_y(i)[j])
                self.assertEquals(e_np[i][j], workspace.read_e(i)[j])
            # Extra X boundary
            self.assertEquals(x_np[i][blocksize], workspace.read_x(i)[blocksize])

if __name__ == '__main__':
    unittest.main()
            
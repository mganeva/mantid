#pylint: disable=no-init
from mantid.api import PythonAlgorithm, AlgorithmFactory, MatrixWorkspaceProperty, PropertyMode, \
    ITableWorkspaceProperty, FileAction, FileProperty
from mantid.kernel import Direction, FloatBoundedValidator, PropertyCriterion, EnabledWhenProperty, logger, Quat, V3D
import mantid.simpleapi as api
from scipy.stats import chisquare
from scipy.optimize import minimize
import numpy as np
import math


class AlignComponents(PythonAlgorithm):
    """ Class to align components
    """

    _optionsList = ["PosX", "PosY", "PosZ", "RotX", "RotY", "RotZ"]
    _optionsDict = {}
    _initialPos = None
    _move = False
    _rotate = False
    _masking = False

    def category(self):
        """ Mantid required
        """
        return "PythonAlgorithms;Diffraction"

    def name(self):
        """ Mantid required
        """
        return "AlignComponents"

    def summary(self):
        """ Mantid required
        """
        return "Align a component by minimising difference to an offset workspace"

    def PyInit(self):
        self.declareProperty(ITableWorkspaceProperty("CalibrationTable",
                                                     "",
                                                     optional=PropertyMode.Mandatory,
                                                     direction=Direction.Input),
                             doc="Calibration table, currently only uses difc")

        self.declareProperty(MatrixWorkspaceProperty("MaskWorkspace",
                                                     "",
                                                     optional=PropertyMode.Optional,
                                                     direction=Direction.Input),
                             doc="Mask workspace")

        self.declareProperty(FileProperty(name="InstrumentFilename",
                                          defaultValue="",
                                          action=FileAction.Load,
                                          extensions=[".xml"]),
                             doc="Instrument filename")

        self.declareProperty("ComponentList", "",
                             direction=Direction.Input,
                             doc="Comma separated list on instrument components to refine.")

        # X position
        self.declareProperty(name="PosX", defaultValue=True,
                             doc="Refine X position")
        condition = EnabledWhenProperty("PosX", PropertyCriterion.IsDefault)
        self.declareProperty(name="MinX", defaultValue=-0.1,
                             validator=FloatBoundedValidator(-10.0, 10.0),
                             doc="Minimum relative X bound (m)")
        self.setPropertySettings("MinX", condition)
        self.declareProperty(name="MaxX", defaultValue=0.1,
                             validator=FloatBoundedValidator(-10.0, 10.0),
                             doc="Maximum relative X bound (m)")
        self.setPropertySettings("MaxX", condition)

        # Y position
        self.declareProperty(name="PosY", defaultValue=True,
                             doc="Refine Y position")
        condition = EnabledWhenProperty("PosY", PropertyCriterion.IsDefault)
        self.declareProperty(name="MinY", defaultValue=-0.1,
                             validator=FloatBoundedValidator(-10.0, 10.0),
                             doc="Minimum relative Y bound (m)")
        self.setPropertySettings("MinY", condition)
        self.declareProperty(name="MaxY", defaultValue=0.1,
                             validator=FloatBoundedValidator(-10.0, 10.0),
                             doc="Maximum relative Y bound (m)")
        self.setPropertySettings("MaxY", condition)

        # Z position
        self.declareProperty(name="PosZ", defaultValue=True,
                             doc="Refine Z position")
        condition = EnabledWhenProperty("PosZ", PropertyCriterion.IsDefault)
        self.declareProperty(name="MinZ", defaultValue=-0.1,
                             validator=FloatBoundedValidator(-10.0, 10.0),
                             doc="Minimum relative Z bound (m)")
        self.setPropertySettings("MinZ", condition)
        self.declareProperty(name="MaxZ", defaultValue=0.1,
                             validator=FloatBoundedValidator(-10.0, 10.0),
                             doc="Maximum relative Z bound (m)")
        self.setPropertySettings("MaxZ", condition)

        # X rotation
        self.declareProperty(name="RotX", defaultValue=True,
                             doc="Refine rotation around X")
        condition = EnabledWhenProperty("RotX", PropertyCriterion.IsDefault)
        self.declareProperty(name="MinRotX", defaultValue=-10.0,
                             validator=FloatBoundedValidator(-90, 90),
                             doc="Minimum relative X rotation (deg)")
        self.setPropertySettings("MinRotX", condition)
        self.declareProperty(name="MaxRotX", defaultValue=10.0,
                             validator=FloatBoundedValidator(-90, 90),
                             doc="Maximum relative X rotation (deg)")
        self.setPropertySettings("MaxRotX", condition)

        # Y rotation
        self.declareProperty(name="RotY", defaultValue=True,
                             doc="Refine rotation around Y")
        condition = EnabledWhenProperty("RotY", PropertyCriterion.IsDefault)
        self.declareProperty(name="MinRotY", defaultValue=-10.0,
                             validator=FloatBoundedValidator(-90, 90),
                             doc="Minimum relative Y rotation (deg)")
        self.setPropertySettings("MinRotY", condition)
        self.declareProperty(name="MaxRotY", defaultValue=10.0,
                             validator=FloatBoundedValidator(-90, 90),
                             doc="Maximum relative Y rotation (deg)")
        self.setPropertySettings("MaxRotY", condition)

        # Z rotation
        self.declareProperty(name="RotZ", defaultValue=True,
                             doc="Refine rotation around Z")
        condition = EnabledWhenProperty("RotZ", PropertyCriterion.IsDefault)
        self.declareProperty(name="MinRotZ", defaultValue=-10.0,
                             validator=FloatBoundedValidator(-90, 90),
                             doc="Minimum relative Z rotation (deg)")
        self.setPropertySettings("MinRotZ", condition)
        self.declareProperty(name="MaxRotZ", defaultValue=10.0,
                             validator=FloatBoundedValidator(-90, 90),
                             doc="Maximum relative Z rotation (deg)")
        self.setPropertySettings("MaxRotZ", condition)

    def validateInputs(self):
        """
        Does basic validation for inputs
        """
        issues = dict()

        calWS = self.getProperty('CalibrationTable').value

        if 'difc' not in calWS.getColumnNames() or 'detid' not in calWS.getColumnNames():
            issues['CalibrationTable'] = "Calibration table requires detid and difc"

        maskWS = self.getProperty("MaskWorkspace").value
        if maskWS != None and maskWS.id() != 'MaskWorkspace':
            issues['MaskWorkspace'] = "MaskWorkspace must be empty or of type \"MaskWorkspace\""

        instrumentWS = api.LoadEmptyInstrument(Filename=self.getProperty("InstrumentFilename").value)
        components = self.getProperty("ComponentList").value.split(',')
        for component in components:
            if instrumentWS.getInstrument().getComponentByName(component) is None:
                issues['ComponentList'] = "Instrument has no component \"" + component + "\""

        if not (self.getProperty("PosX").value + self.getProperty("PosY").value + self.getProperty("PosZ").value +
                    self.getProperty("RotX").value + self.getProperty("RotY").value + self.getProperty("RotZ").value):
            issues["PosX"] = "You must refine something"

        return issues

    def PyExec(self):
        calWS = self.getProperty('CalibrationTable').value
        maskWS = self.getProperty("MaskWorkspace").value

        if maskWS != None:
            self._masking = True
            mask = maskWS.extractY().flatten()

        difc = calWS.column('difc')
        if self._masking:
            difc = np.ma.masked_array(difc, mask)

        instrumentWS = api.LoadEmptyInstrument(Filename=self.getProperty("InstrumentFilename").value)

        components = self.getProperty("ComponentList").value.split(',')

        for opt in self._optionsList:
            self._optionsDict[opt] = self.getProperty(opt).value

        if self._optionsDict["PosX"] or self._optionsDict["PosY"] or self._optionsDict["PosZ"]:
            self._move = True

        if self._optionsDict["RotX"] or self._optionsDict["RotY"] or self._optionsDict["RotZ"]:
            self._rotate = True

        for component in components:
            comp = instrumentWS.getInstrument().getComponentByName(component)
            firstDetID = self._getFirstDetID(comp)
            lastDetID = self._getLastDetID(comp)

            eulerAngles = comp.getRotation().getEulerAngles()

            logger.notice("Working on " + comp.getFullName() +
                          " Starting position is " + str(comp.getPos()) +
                          " Starting rotation is " + str(eulerAngles))

            x0List = []
            self._initialPos = [comp.getPos().getX(), comp.getPos().getY(), comp.getPos().getZ(),
                                eulerAngles[0], eulerAngles[1], eulerAngles[2]]

            boundsList = []

            if self._masking:
                mask_out = mask[firstDetID:lastDetID + 1]
            else:
                mask_out = None

            if self._optionsDict["PosX"]:
                x0List.append(self._initialPos[0])
                boundsList.append((self._initialPos[0] + self.getProperty("MinX").value,
                                   self._initialPos[0] + self.getProperty("MaxX").value))
            if self._optionsDict["PosY"]:
                x0List.append(self._initialPos[1])
                boundsList.append((self._initialPos[1] + self.getProperty("MinY").value,
                                   self._initialPos[1] + self.getProperty("MaxY").value))
            if self._optionsDict["PosZ"]:
                x0List.append(self._initialPos[2])
                boundsList.append((self._initialPos[2] + self.getProperty("MinZ").value,
                                   self._initialPos[2] + self.getProperty("MaxZ").value))
            if self._optionsDict["RotX"]:
                x0List.append(self._initialPos[3])
                boundsList.append((self._initialPos[3] + self.getProperty("MinRotX").value,
                                   self._initialPos[3] + self.getProperty("MaxRotX").value))
            if self._optionsDict["RotY"]:
                x0List.append(self._initialPos[4])
                boundsList.append((self._initialPos[4] + self.getProperty("MinRotY").value,
                                   self._initialPos[4] + self.getProperty("MaxRotY").value))
            if self._optionsDict["RotZ"]:
                x0List.append(self._initialPos[5])
                boundsList.append((self._initialPos[5] + self.getProperty("MinRotZ").value,
                                   self._initialPos[5] + self.getProperty("MaxRotZ").value))

            results = minimize(self._minimisation_func, x0=x0List,
                               args=(instrumentWS,
                                     component,
                                     firstDetID,
                                     lastDetID,
                                     difc[firstDetID:lastDetID + 1],
                                     mask_out),
                               bounds=boundsList)

            # Apply the results to the output workspace
            out = self._mapOptions(results.x)

            if self._move:
                api.MoveInstrumentComponent(instrumentWS, component, X=out[0], Y=out[1], Z=out[2],
                                            RelativePosition=False)

            if self._rotate:
                (rw, rx, ry, rz) = self._eulerToAngleAxis(out[3], out[4], out[5])
                api.RotateInstrumentComponent(instrumentWS, component, X=rx, Y=ry, Z=rz, Angle=rw,
                                              RelativeRotation=False)

            # Need to grab the component again, as things have changed
            comp = instrumentWS.getInstrument().getComponentByName(component)
            logger.notice("Finshed " + comp.getFullName() +
                          " Final position is " + str(comp.getPos()) +
                          " Final rotation is " + str(comp.getRotation().getEulerAngles()))

    def _minimisation_func(self, x0, ws, component, firstDetID, lastDetID, difc, mask):
        x = self._mapOptions(x0)

        if self._move:
            api.MoveInstrumentComponent(ws, component, X=x[0], Y=x[1], Z=x[2], RelativePosition=False)

        if self._rotate:
            (rw, rx, ry, rz) = self._eulerToAngleAxis(x[3], x[4], x[5])
            api.RotateInstrumentComponent(ws, component, X=rx, Y=ry, Z=rz, Angle=rw, RelativeRotation=False)

        ws = api.CalculateDIFC(ws)

        difc_new = ws.extractY().flatten()[firstDetID:lastDetID + 1]

        if self._masking and mask is not None:
            difc_new = np.ma.masked_array(difc_new, mask)

        return chisquare(f_obs=difc, f_exp=difc_new)[0]

    def _getFirstDetID(self, component):
        """
        recursive search to find first detID
        """
        if component.type() == 'DetectorComponent':
            return component.getID()
        else:
            return self._getFirstDetID(component[0])

    def _getLastDetID(self, component):
        """
        recursive search to find last detID
        """
        if component.type() == 'DetectorComponent':
            return component.getID()
        else:
            return self._getLastDetID(component[component.nelements() - 1])

    def _mapOptions(self, x):
        """
        Creates an array combining the refining and constant variables
        """
        x0_index = 0
        out = []
        for opt in self._optionsList:
            if self._optionsDict[opt]:
                out.append(x[x0_index])
                x0_index += 1
            else:
                out.append(self._initialPos[self._optionsList.index(opt)])
        return out

    def _eulerToQuat(self, a, b, c, convention="YZX"):
        """
        Convert Euler angles to a quaternion
        """
        getV3D = {'X': V3D(1, 0, 0), 'Y': V3D(0, 1, 0), 'Z': V3D(0, 0, 1)}
        q1 = Quat(a, getV3D[convention[0]])
        q2 = Quat(b, getV3D[convention[1]])
        q3 = Quat(c, getV3D[convention[2]])
        return q1 * q2 * q3

    def _eulerToAngleAxis(self, a, b, c, convention="YZX"):
        """
        Convert Euler angles to a angle rotation around an axis
        """
        q = self._eulerToQuat(a, b, c, convention)
        if q[0] == 1:
            return 0, 0, 0, 1
        deg = math.acos(q[0])
        s = math.sin(deg)
        deg *= 360.0 / math.pi
        ax0 = q[1] / s
        ax1 = q[2] / s
        ax2 = q[3] / s
        return deg, ax0, ax1, ax2


AlgorithmFactory.subscribe(AlignComponents)

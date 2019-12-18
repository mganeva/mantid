# Mantid Repository : https://github.com/mantidproject/mantid
#
# Copyright &copy; 2019 ISIS Rutherford Appleton Laboratory UKRI,
#     NScD Oak Ridge National Laboratory, European Spallation Source
#     & Institut Laue - Langevin
# SPDX - License - Identifier: GPL - 3.0 +

from __future__ import (absolute_import, division, print_function)

from os import path
from os import makedirs

from mantid.simpleapi import logger, Load, EnggVanadiumCorrections, SaveNexus
from mantid.simpleapi import AnalysisDataService as Ads

from Engineering.gui.engineering_diffraction.tabs.common import path_handling

VANADIUM_INPUT_WORKSPACE_NAME = "engggui_vanadium_ws"
CURVES_WORKSPACE_NAME = "engggui_vanadium_curves"
INTEGRATED_WORKSPACE_NAME = "engggui_vanadium_integration"

VANADIUM_DIRECTORY_NAME = "Vanadium_Runs"

SAVED_FILE_CURVE_SUFFIX = "_precalculated_vanadium_run_bank_curves.nxs"
SAVED_FILE_INTEG_SUFFIX = "_precalculated_vanadium_run_integration.nxs"


def fetch_correction_workspaces(vanadium_path, instrument, rb_num=""):
    """
    Fetch workspaces from the file system or create new ones.
    :param vanadium_path: The path to the requested vanadium run raw data.
    :param instrument: The instrument the data came from.
    :param rb_num: A user identifier, usually an experiment number.
    :return: The resultant integration and curves workspaces.
    """
    vanadium_number = path_handling.get_run_number_from_path(vanadium_path, instrument)
    integ_path, curves_path = _generate_saved_workspace_file_paths(vanadium_number)
    if path.exists(curves_path) and path.exists(integ_path):  # Check if the cached files exist.
        try:
            integ_workspace = Load(Filename=integ_path, OutputWorkspace=INTEGRATED_WORKSPACE_NAME)
            curves_workspace = Load(Filename=curves_path, OutputWorkspace=CURVES_WORKSPACE_NAME)
            if rb_num:
                user_integ, user_curves = _generate_saved_workspace_file_paths(vanadium_number,
                                                                               rb_num=rb_num)
                if not path.exists(user_integ) and not path.exists(user_curves):
                    _save_correction_files(integ_workspace, user_integ, curves_workspace,
                                           user_curves)
            return integ_workspace, curves_workspace
        except RuntimeError as e:
            logger.error(
                "Problem loading existing vanadium calculations. Creating new cached files. Description: "
                + str(e))
    integ_workspace, curves_workspace = _calculate_vanadium_correction(vanadium_path)
    _save_correction_files(integ_workspace, integ_path, curves_workspace, curves_path)
    if rb_num:
        user_integ, user_curves = _generate_saved_workspace_file_paths(vanadium_number,
                                                                       rb_num=rb_num)
        _save_correction_files(integ_workspace, user_integ, curves_workspace, user_curves)
    return integ_workspace, curves_workspace


def check_workspaces_exist():
    return Ads.doesExist(CURVES_WORKSPACE_NAME) and Ads.doesExist(INTEGRATED_WORKSPACE_NAME)


def _calculate_vanadium_correction(vanadium_path):
    """
    Runs the vanadium correction algorithm.
    :param vanadium_path: The path to the vanadium data.
    :return: The integrated workspace and the curves generated by the algorithm.
    """
    try:
        Load(Filename=vanadium_path, OutputWorkspace=VANADIUM_INPUT_WORKSPACE_NAME)
    except Exception as e:
        logger.error("Error when loading vanadium sample data. "
                     "Could not run Load algorithm with vanadium run number: " +
                     str(vanadium_path) + ". Error description: " + str(e))
        raise RuntimeError
    EnggVanadiumCorrections(VanadiumWorkspace=VANADIUM_INPUT_WORKSPACE_NAME,
                            OutIntegrationWorkspace=INTEGRATED_WORKSPACE_NAME,
                            OutCurvesWorkspace=CURVES_WORKSPACE_NAME)
    Ads.remove(VANADIUM_INPUT_WORKSPACE_NAME)
    integrated_workspace = Ads.Instance().retrieve(INTEGRATED_WORKSPACE_NAME)
    curves_workspace = Ads.Instance().retrieve(CURVES_WORKSPACE_NAME)
    return integrated_workspace, curves_workspace


def _save_correction_files(integration_workspace, integration_path, curves_workspace, curves_path):
    """
    Attempt to save the created workspaces to the filesystem.
    :param integration_workspace: The workspace for the vanadium integration.
    :param integration_path: The path to save the integration workspace to.
    :param curves_workspace: The workspace for the vanadium curves.
    :param curves_path: The path to save the curves workspace to.
    """
    try:
        SaveNexus(InputWorkspace=integration_workspace, Filename=integration_path)
        SaveNexus(InputWorkspace=curves_workspace, Filename=curves_path)
    except RuntimeError as e:  # If the files cannot be saved, continue with the execution of the algorithm anyway.
        logger.error(
            "Vanadium Correction files could not be saved to the filesystem. Description: " +
            str(e))
        return


def _generate_saved_workspace_file_paths(vanadium_number, rb_num=""):
    """
    Generate file paths based on a vanadium run number.
    :param vanadium_number: The number of the vanadium run.
    :param rb_num: User identifier, usually an experiment number.
    :return: The full path to the file.
    """
    integrated_filename = vanadium_number + SAVED_FILE_INTEG_SUFFIX
    curves_filename = vanadium_number + SAVED_FILE_CURVE_SUFFIX
    if rb_num:
        vanadium_dir = path.join(path_handling.OUT_FILES_ROOT_DIR, "User", rb_num,
                                 VANADIUM_DIRECTORY_NAME)
    else:
        vanadium_dir = path.join(path_handling.OUT_FILES_ROOT_DIR, VANADIUM_DIRECTORY_NAME)
    if not path.exists(vanadium_dir):
        makedirs(vanadium_dir)
    return path.join(vanadium_dir, integrated_filename), path.join(vanadium_dir, curves_filename)

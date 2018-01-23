#ifndef MANTID_CUSTOMINTERFACES_QTREFLSETTINGSVIEW_H_
#define MANTID_CUSTOMINTERFACES_QTREFLSETTINGSVIEW_H_

#include "IReflSettingsView.h"
#include "ui_ReflSettingsWidget.h"
#include <memory>
#include "ExperimentOptionDefaults.h"
#include "InstrumentOptionDefaults.h"

namespace MantidQt {
namespace CustomInterfaces {

// Forward decs
class IReflSettingsPresenter;

/** QtReflSettingsView : Provides an interface for the "Settings" widget in the
ISIS Reflectometry interface.

Copyright &copy; 2016 ISIS Rutherford Appleton Laboratory, NScD Oak Ridge
National Laboratory & European Spallation Source

This file is part of Mantid.

Mantid is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

Mantid is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

File change history is stored at: <https://github.com/mantidproject/mantid>
Code Documentation is available at: <http://doxygen.mantidproject.org>
*/
class QtReflSettingsView : public QWidget, public IReflSettingsView {
  Q_OBJECT
public:
  /// Constructor
  explicit QtReflSettingsView(int group, QWidget *parent = nullptr);
  /// Destructor
  ~QtReflSettingsView() override;
  /// Returns the presenter managing this view
  IReflSettingsPresenter *getPresenter() const override;
  /// Returns global options for 'Stitch1DMany'
  std::string getStitchOptions() const override;
  /// Return selected analysis mode
  std::string getAnalysisMode() const override;
  /// Return transmission runs
  std::string getTransmissionRuns() const override;
  /// Return start overlap for transmission runs
  std::string getStartOverlap() const override;
  /// Return end overlap for transmission runs
  std::string getEndOverlap() const override;
  /// Return selected polarisation corrections
  std::string getPolarisationCorrections() const override;
  /// Return CRho
  std::string getCRho() const override;
  /// Return CAlpha
  std::string getCAlpha() const override;
  /// Return CAp
  std::string getCAp() const override;
  /// Return Cpp
  std::string getCPp() const override;
  /// Return momentum transfer limits
  std::string getMomentumTransferStep() const override;
  /// Return scale factor
  std::string getScaleFactor() const override;
  /// Return integrated monitors option
  std::string getIntMonCheck() const override;
  /// Return monitor integral wavelength min
  std::string getMonitorIntegralMin() const override;
  /// Return monitor integral wavelength max
  std::string getMonitorIntegralMax() const override;
  /// Return monitor background wavelength min
  std::string getMonitorBackgroundMin() const override;
  /// Return monitor background wavelength max
  std::string getMonitorBackgroundMax() const override;
  /// Return wavelength min
  std::string getLambdaMin() const override;
  /// Return wavelength max
  std::string getLambdaMax() const override;
  /// Return I0MonitorIndex
  std::string getI0MonitorIndex() const override;
  /// Return processing instructions
  std::string getProcessingInstructions() const override;
  /// Return selected detector correction type
  std::string getDetectorCorrectionType() const override;
  /// Return selected summation type
  std::string getSummationType() const override;
  /// Return selected reduction type
  std::string getReductionType() const override;
  /// Set the status of whether polarisation corrections should be enabled
  void setIsPolCorrEnabled(bool enable) const override;
  /// Set default values for experiment and instrument settings
  void setExpDefaults(ExperimentOptionDefaults defaults) override;
  void setInstDefaults(InstrumentOptionDefaults defaults) override;
  /// Check if experiment settings are enabled
  bool experimentSettingsEnabled() const override;
  /// Check if instrument settings are enabled
  bool instrumentSettingsEnabled() const override;
  /// Check if detector correction is enabled
  bool detectorCorrectionEnabled() const override;
  /// Creates hints for 'Stitch1DMany'
  void
  createStitchHints(const std::map<std::string, std::string> &hints) override;

  void showOptionLoadErrors(
      std::vector<InstrumentParameterTypeMissmatch> const &errors,
      std::vector<MissingInstrumentParameterValue> const &missingValues)
      override;

public slots:
  /// Request presenter to obtain default values for settings
  void requestExpDefaults() const;
  void requestInstDefaults() const;
  void summationTypeChanged(int reductionTypeIndex);
  /// Sets enabled status for polarisation corrections and parameters
  void setPolarisationOptionsEnabled(bool enable) override;
  void setReductionTypeEnabled(bool enable) override;
  void setDetectorCorrectionEnabled(bool enable) override;
  void notifySettingsChanged();

private:
  /// Initialise the interface
  void initLayout();
  QLineEdit &stitchOptionsLineEdit() const;
  void setSelected(QComboBox &box, std::string const &str);
  void setText(QLineEdit &lineEdit, int value);
  void setText(QLineEdit &lineEdit, double value);
  void setText(QLineEdit &lineEdit, std::string const &value);
  void setText(QLineEdit &lineEdit, boost::optional<int> value);
  void setText(QLineEdit &lineEdit, boost::optional<double> value);
  void setText(QLineEdit &lineEdit, boost::optional<std::string> const &value);
  void setChecked(QCheckBox &checkBox, bool checked);
  std::string getText(QLineEdit const &lineEdit) const;
  std::string getText(QComboBox const &box) const;
  void connectChangeListeners();
  void connectExperimentSettingsChangeListeners();
  void connectInstrumentSettingsChangeListeners();
  void connectSettingsChange(QLineEdit *edit);
  void connectSettingsChange(QComboBox *edit);
  void connectSettingsChange(QCheckBox *edit);
  void connectSettingsChange(QGroupBox *edit);

  /// The widget
  Ui::ReflSettingsWidget m_ui;
  /// The presenter
  std::unique_ptr<IReflSettingsPresenter> m_presenter;
  /// Whether or not polarisation corrections should be enabled
  mutable bool m_isPolCorrEnabled;
};
} // namespace Mantid
} // namespace CustomInterfaces

#endif /* MANTID_CUSTOMINTERFACES_QTREFLSETTINGSVIEW_H_ */

#ifndef MANTID_ALGORITHMS_PDDETERMINECHARACTERIZATIONS_H_
#define MANTID_ALGORITHMS_PDDETERMINECHARACTERIZATIONS_H_

#include "MantidKernel/System.h"
#include "MantidAPI/Algorithm.h"
#include "MantidAPI/ITableWorkspace_fwd.h"

namespace Mantid {

namespace Kernel {
/// forward declaration
class PropertyMantager;
/// Typedef for a shared pointer to a PropertyManager
typedef boost::shared_ptr<PropertyManager> PropertyManager_sptr;
}

namespace Algorithms {

/** PDDetermineCharacterizations

  Copyright &copy; 2015 ISIS Rutherford Appleton Laboratory, NScD Oak Ridge
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
class DLLExport PDDetermineCharacterizations : public API::Algorithm {
public:
  PDDetermineCharacterizations();
  virtual ~PDDetermineCharacterizations();

  virtual const std::string name() const;
  virtual int version() const;
  virtual const std::string category() const;
  virtual const std::string summary() const;
  virtual std::map<std::string, std::string> validateInputs();

private:
  double getLogValue(API::Run &run, const std::string &propName);
  void getInformationFromTable(const double frequency, const double wavelength);
  void setDefaultsInPropManager();
  void overrideRunNumProperty(const std::string &inputName,
                              const std::string &propName);
  void init();
  void exec();

  Kernel::PropertyManager_sptr m_propertyManager;
  API::ITableWorkspace_sptr m_characterizations;
};

} // namespace Algorithms
} // namespace Mantid

#endif /* MANTID_ALGORITHMS_PDDETERMINECHARACTERIZATIONS_H_ */

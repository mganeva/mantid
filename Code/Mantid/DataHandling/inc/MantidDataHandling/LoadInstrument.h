#ifndef MANTID_DATAHANDLING_LOADINSTRUMENT_H_
#define MANTID_DATAHANDLING_LOADINSTRUMENT_H_

//----------------------------------------------------------------------
// Includes
//----------------------------------------------------------------------
#include "MantidDataHandling/DataHandlingCommand.h"
#include "MantidKernel/Logger.h"

namespace Mantid
{
namespace DataHandling
{
/** @class LoadInstrument LoadInstrument.h DataHandling/LoadInstrument.h

    Loads instrument data from a file and adds it to a workspace. 
	The current implementation uses a simple data structure that will be replaced later.
	LoadInstrument is intended to be used as a child algorithm of 
	other Loadxxx algorithms, rather than being used directly.
	LoadInstrument is an algorithm and as such inherits
    from the Algorithm class, via DataHandlingCommand, and overrides
    the init(), exec() & final() methods.
    
    Required Properties:
       <UL>
       <LI> Filename - The name of and path to the input RAW file </LI>
       <LI> InputWorkspace - The name of the workspace in which to use as a basis for any data to be added, a blank 2dWorkspace will be used as a default.</LI>
       <LI> OutputWorkspace - The name of the workspace in which to store the imported data </LI>
       </UL>

    @author Nick Draper, Tessella Support Services plc
    @date 19/11/2007
    
    Copyright &copy; 2007 STFC Rutherford Appleton Laboratories

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

    File change history is stored at: <https://svn.mantidproject.org/mantid/trunk/Code/Mantid>    
*/
  class DLLExport LoadInstrument : public DataHandlingCommand
  {
  public:
    /// Default constructor
    LoadInstrument();

    /// Destructor
    ~LoadInstrument() {}
    
  private:

    /// Overwrites Algorithm method. Does nothing at present
    void init();
    
    /// Overwrites Algorithm method
    void exec();
    
    /// Overwrites Algorithm method. Does nothing at present
    void final();
    
    /// The name and path of the input file
    std::string m_filename;
    
    ///static reference to the logger class
    static Kernel::Logger& g_log;
  };

} // namespace DataHandling
} // namespace Mantid

#endif /*MANTID_DATAHANDLING_LOADINSTRUMENT_H_*/


// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2019 ISIS Rutherford Appleton Laboratory UKRI,
//     NScD Oak Ridge National Laboratory, European Spallation Source
//     & Institut Laue - Langevin
// SPDX - License - Identifier: GPL - 3.0 +

#ifndef MANTID_API_CITATION_CITATION_H_
#define MANTID_API_CITATION_CITATION_H_

#include "MantidAPI/CitationConstructorHelpers.h"
#include "MantidAPI/DllConfig.h"

#include <string>
#include <vector>

#include <nexus/NeXusFile.hpp>

namespace Mantid {
namespace API {

class MANTID_API_DLL Citation {
public:
  Citation(const BaseCitation &cite);
  Citation(::NeXus::File *file, const std::string &group);
  Citation(const std::string &doi = "", const std::string &bibtex = "",
           const std::string &endnote = "", const std::string &url = "",
           const std::string &description = "");

  // Needed for future Set constructiom
  bool operator==(const Citation &rhs) const;

  const std::string &description() const;
  const std::string &url() const;
  const std::string &doi() const;
  const std::string &bibtex() const;
  const std::string &endnote() const;

  void loadNexus(::NeXus::File *file, const std::string &group);
  void saveNexus(::NeXus::File *file, const std::string &group);

private:
  std::string m_doi;
  std::string m_bibtex;
  std::string m_endnote;
  std::string m_url;
  std::string m_description;
};
} // namespace API
} // namespace Mantid

#endif /* MANTID_API_CITATION_CITATION_H_ */
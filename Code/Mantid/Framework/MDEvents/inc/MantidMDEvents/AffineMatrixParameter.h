#ifndef MANTID_MDEVENTS_AFFINE_MATRIX_PARAMETER
#define MANTID_MDEVENTS_AFFINE_MATRIX_PARAMETER

#include "MantidKernel/System.h"
#include "MantidAPI/ImplicitFunctionParameter.h"
#include "MantidKernel/Matrix.h"
#include "MantidGeometry/MDGeometry/MDTypes.h"

namespace Mantid
{
  namespace MDEvents
  {
    /// Convenience typedef for a specific matrix type.
    typedef Mantid::Kernel::Matrix<coord_t> AffineMatrixType;

    /** Type to wrap an affine matrix and allow serialization via xml.
    *
    * @author Owen Arnold
    * @date 20/07/2011
    */
    class DLLExport AffineMatrixParameter : public Mantid::API::ImplicitFunctionParameter
    {
    public:

      //ImplcitFunctionParameter Methods.
      virtual std::string getName() const;
      virtual bool isValid() const;
      virtual std::string toXMLString() const;
      virtual AffineMatrixParameter* clone() const;
      void setMatrix(const AffineMatrixType newMatrix);
      AffineMatrixParameter(size_t outD, size_t inD);
      AffineMatrixParameter(const AffineMatrixParameter&);
      AffineMatrixParameter& operator=(const AffineMatrixParameter& other);
      ~AffineMatrixParameter();

      coord_t ** getRawMatrix();
      AffineMatrixType  getAffineMatrix() const; 
      
      /*
        Gets the type parameter name.
        @return parameter name.
      */
      static std::string parameterName()
      {
        return "AffineMatrixParameter";
      }

    private:

      void copyRawMatrix();

      /// Raw matrix used for speed.
      coord_t ** rawMatrix;

      /// Affine matrix.
      AffineMatrixType affineMatrix;
    };
  }
}

#endif
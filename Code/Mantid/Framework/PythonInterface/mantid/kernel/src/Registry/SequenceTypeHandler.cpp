//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include "MantidPythonInterface/kernel/Registry/SequenceTypeHandler.h"
#include "MantidPythonInterface/kernel/Converters/PySequenceToVector.h"
#include "MantidPythonInterface/kernel/Converters/NDArrayToVector.h"
#include "MantidKernel/IPropertyManager.h"

// See http://docs.scipy.org/doc/numpy/reference/c-api.array.html#PY_ARRAY_UNIQUE_SYMBOL
#define PY_ARRAY_UNIQUE_SYMBOL KERNEL_ARRAY_API
#define NO_IMPORT_ARRAY
#include <numpy/arrayobject.h>

#include <boost/python/extract.hpp>

namespace Mantid
{
  namespace PythonInterface
  {
    namespace Registry
    {
      /**
       * Set function to handle Python -> C++ calls to a property manager and get the correct type
       * @param alg :: A pointer to an IPropertyManager
       * @param name :: The name of the property
       * @param value :: A boost python object that stores the container values
       */
      template<typename ContainerType>
      void SequenceTypeHandler<ContainerType>::set(Kernel::IPropertyManager* alg, const std::string &name,
                                                   const boost::python::object & value)
      {
        using boost::python::len;
        typedef typename ContainerType::value_type DestElementType;

        // Check for a container-like object
        Py_ssize_t length(0);
        try
        {
         length = len(value);
        }
        catch(boost::python::error_already_set&)
        {
          throw std::invalid_argument(name + " property requires a list/array type but found a " + value.ptr()->ob_type->tp_name);
        }

        // numpy arrays requires special handling to extract their types. Hand-off to a more appropriate handler
        if( PyArray_Check(value.ptr()) )
        {
          alg->setProperty(name, Converters::NDArrayToVector<DestElementType>(value)());
        }
        else if( PySequence_Check(value.ptr()) )
        {
          alg->setProperty(name, Converters::PySequenceToVector<DestElementType>(value)());
        }
        else
        {
          throw std::invalid_argument(std::string("Unknown sequence type \"") + value.ptr()->ob_type->tp_name
                                        + "\" found when setting " + name + " property.");
        }
      }

      //-----------------------------------------------------------------------
      // Concrete instantiations
      //-----------------------------------------------------------------------
      #define INSTANTIATE(ElementType)\
        template DLLExport struct SequenceTypeHandler<std::vector<ElementType> >;

      INSTANTIATE(int);
      INSTANTIATE(long);
      INSTANTIATE(long long);
      INSTANTIATE(unsigned int);
      INSTANTIATE(unsigned long);
      INSTANTIATE(unsigned long long);
      INSTANTIATE(double);
      INSTANTIATE(std::string);
      INSTANTIATE(bool);

    }
  }
}

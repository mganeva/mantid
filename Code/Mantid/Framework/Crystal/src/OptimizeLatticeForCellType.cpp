/*WIKI* 


*WIKI*/
#include "MantidCrystal/OptimizeLatticeForCellType.h"
#include "MantidCrystal/GSLFunctions.h"
#include "MantidDataObjects/PeaksWorkspace.h"
#include "MantidAPI/FileProperty.h"
#include "MantidAPI/FunctionFactory.h"
#include "MantidAPI/WorkspaceValidators.h"
#include "MantidAPI/IPeakFunction.h"
#include "MantidKernel/VectorHelper.h"
#include "MantidKernel/ListValidator.h"
#include "MantidKernel/BoundedValidator.h"
#include "MantidGeometry/Crystal/OrientedLattice.h"
#include "MantidGeometry/Crystal/IndexingUtils.h"
#include "MantidGeometry/Instrument/RectangularDetector.h"
#include <boost/math/special_functions/fpclassify.hpp>
#include <fstream>
#include <iomanip>
#include <ostream>
#include <sstream>

using namespace Mantid::Geometry;

namespace Mantid
{
  namespace Crystal
  {
    Kernel::Logger& OptimizeLatticeForCellType::g_log =
                        Kernel::Logger::get("OptimizeLatticeForCellType");

    // Register the class into the algorithm factory
    DECLARE_ALGORITHM(OptimizeLatticeForCellType)
    
    /// Sets documentation strings for this algorithm
    void OptimizeLatticeForCellType::initDocs()
    {
      this->setWikiSummary("Optimize lattice parameters for cell type.");
      this->setOptionalMessage("Optimize lattice parameters for cell type.");
    }
    
    using namespace Kernel;
    using namespace API;
    using std::size_t;
    using namespace DataObjects;

    /// Constructor
    OptimizeLatticeForCellType::OptimizeLatticeForCellType()
    {
    }

    /// Destructor
    OptimizeLatticeForCellType::~OptimizeLatticeForCellType()
    {}
  

  
    //-----------------------------------------------------------------------------------------
    /** Initialisation method. Declares properties to be used in algorithm.
     */
    void OptimizeLatticeForCellType::init()
    {

    declareProperty(new WorkspaceProperty<PeaksWorkspace>("PeaksWorkspace","",Direction::InOut),
        "An input PeaksWorkspace with an instrument.");
    std::vector<std::string> cellTypes;
    cellTypes.push_back("Cubic" );
    cellTypes.push_back("Tetragonal" );
    cellTypes.push_back("Orthorhombic");
    cellTypes.push_back("Hexagonal");
    cellTypes.push_back("Rhombohedral");
    cellTypes.push_back("Monoclinic ( a unique )");
    cellTypes.push_back("Monoclinic ( b unique )");
    cellTypes.push_back("Monoclinic ( c unique )");
    cellTypes.push_back("Triclinic");
    declareProperty("CellType", cellTypes[0],boost::make_shared<StringListValidator>(cellTypes),
      "Select the cell type.");
    declareProperty( "Apply", false, "Re-index the peaks");
    declareProperty( "Tolerance", 0.12, "Indexing Tolerance");
    declareProperty("EdgePixels",0, "The number of pixels where peaks are removed at edges. " );
    declareProperty("OutputChi2", 0.0,Direction::Output);

      //Disable default gsl error handler (which is to call abort!)
      gsl_set_error_handler_off();
    }

    //-----------------------------------------------------------------------------------------
    /** Executes the algorithm
     *
     *  @throw Exception::FileError If the grouping file cannot be opened or read successfully
     */
    void OptimizeLatticeForCellType::exec()
    {
      bool   apply          = this->getProperty("Apply");
      double tolerance      = this->getProperty("Tolerance");
      std::string edge 		= this->getProperty("EdgePixels");
      std::string par[6];
      std::string inname = getProperty("PeaksWorkspace");
      par[0] = inname;
      std::string type = getProperty("CellType");
      par[1] = type;
      par[2] = edge;
      DataObjects::PeaksWorkspace_sptr ws = getProperty("PeaksWorkspace");

      std::vector<double> x;
    
      // finally do the optimization
    
      size_t nopt = 1;
      if(type.compare(0,2,"Te")==0)nopt = 2;
      else if(type.compare(0,2,"Or")==0)nopt = 3;
      else if(type.compare(0,2,"Rh")==0)nopt = 2;
      else if(type.compare(0,2,"He")==0)nopt = 2;
      else if(type.compare(0,2,"Mo")==0)nopt = 4;
      else if(type.compare(0,2,"Tr")==0)nopt = 6;

      const DblMatrix UB = ws->sample().getOrientedLattice().getUB();
      std::vector<double>lat(6);
      IndexingUtils::GetLatticeParameters( UB, lat);
      // initialize parameters for optimization
      x.push_back(lat[0]);
      if(type.compare(0,2,"Te")==0)x.push_back(lat[2]);
      else if(type.compare(0,2,"Or")==0)
      {
    	  x.push_back( lat[1]);
    	  x.push_back(lat[2]);
      }
      else if(type.compare(0,2,"Rh")==0)x.push_back( lat[3]);
      else if(type.compare(0,2,"He")==0)x.push_back( lat[2]);
      else if(type.compare(0,2,"Mo")==0)
      {
    	  x.push_back(lat[1]);
    	  x.push_back(lat[2]);
        if(type.compare(13,1,"a")==0)x.push_back( lat[3]);
        else if(type.compare(13,1,"b")==0)x.push_back(lat[4]);
        else if(type.compare(13,1,"c")==0)x.push_back(lat[5]);
      }
      else if(type.compare(0,2,"Tr")==0)for ( size_t i = 1; i < nopt; i++ )x.push_back( lat[i]);
      size_t n_peaks = ws->getNumberPeaks();
      MatrixWorkspace_sptr data = WorkspaceFactory::Instance().create(
                 std::string("Workspace2D"), 1, n_peaks, n_peaks);
      for(size_t i = 0; i < data->blocksize(); i++)
      {
          data->dataX(0)[i] = static_cast<double>(i);
          data->dataY(0)[i] = 0.0;
          data->dataE(0)[i] = 1.0;
      }

      std::ostringstream fun_str;
      fun_str << "name=LatticeErrors";
      for ( size_t i = 0; i < nopt; i++ )fun_str << ",p" << i <<"="<<x[i];

      IAlgorithm_sptr fit_alg;
      try
      {
        fit_alg = createChildAlgorithm("Fit", -1, -1, false);
      } catch (Exception::NotFoundError&)
      {
        g_log.error("Can't locate Fit algorithm");
        throw ;
      }

      fit_alg->setPropertyValue("Function", fun_str.str());
      fit_alg->setProperty("InputWorkspace", data);
      fit_alg->setProperty("WorkspaceIndex", 0);
      fit_alg->setProperty("MaxIterations", 5000);
      fit_alg->setProperty("CreateOutput", true);
      fit_alg->setProperty("Output", "fit");
      fit_alg->executeAsChildAlg();
      MatrixWorkspace_sptr fitWS = fit_alg->getProperty("OutputWorkspace");

      double chisq = fit_alg->getProperty("OutputChi2overDoF");
      std::vector<double> Params;
      IFunction_sptr out = fit_alg->getProperty("Function");

      Params.push_back(out->getParameter("p0"));
      Params.push_back(out->getParameter("p1"));
      Params.push_back(out->getParameter("p2"));
      Params.push_back(out->getParameter("p3"));
      Params.push_back(out->getParameter("p4"));
      Params.push_back(out->getParameter("p5"));


      std::vector<double> sigabc(Params.size());
      OrientedLattice latt=ws->mutableSample().getOrientedLattice();
      DblMatrix UBnew = latt.getUB();
      const std::vector<Peak> &peaks = ws->getPeaks();
      std::vector<V3D> hkl_vector;

      for ( size_t i = 0; i < n_peaks; i++ )
      {
        hkl_vector.push_back(peaks[i].getHKL());
      }
      Calculate_Errors(n_peaks, inname, type, Params, atoi(edge.c_str()), sigabc, chisq);
      OrientedLattice o_lattice;
      o_lattice.setUB( UBnew );

      if (type == "Cubic")
      {
    	o_lattice.setError(sigabc[0],sigabc[0],sigabc[0],0,0,0);
      }
      else if (type == "Tetragonal")
      {
    	o_lattice.setError(sigabc[0],sigabc[0],sigabc[1],0,0,0);
      }
      else if (type == "Orthorhombic")
      {
    	o_lattice.setError(sigabc[0],sigabc[1],sigabc[2],0,0,0);
      }
      else if (type == "Rhombohedral")
      {
    	o_lattice.setError(sigabc[0],sigabc[0],sigabc[0],sigabc[1],sigabc[1],sigabc[1]);
      }
      else if (type == "Hexagonal")
      {
    	o_lattice.setError(sigabc[0],sigabc[0],sigabc[1],0,0,0);
      }
      else if (type == "Monoclinic ( a unique )")
      {
    	o_lattice.setError(sigabc[0],sigabc[1],sigabc[2],sigabc[3],0,0);
      }
      else if (type == "Monoclinic ( b unique )")
      {
    	o_lattice.setError(sigabc[0],sigabc[1],sigabc[2],0,sigabc[3],0);

      }
      else if (type == "Monoclinic ( c unique )")
      {
    	o_lattice.setError(sigabc[0],sigabc[1],sigabc[2],0,0,sigabc[3]);
      }
      else if (type == "Triclinic")
      {
    	o_lattice.setError(sigabc[0],sigabc[1],sigabc[2],sigabc[3],sigabc[4],sigabc[5]);
      }

      // Show the modified lattice parameters
      g_log.notice() << o_lattice << "\n";

      ws->mutableSample().setOrientedLattice( new OrientedLattice(o_lattice) );

      setProperty("OutputChi2", chisq);

      if ( apply )
      {
		  // Reindex peaks with new UB
		  Mantid::API::IAlgorithm_sptr alg = createChildAlgorithm("IndexPeaks");
		  alg->setPropertyValue("PeaksWorkspace", inname);
		  alg->setProperty("Tolerance", tolerance);
		  alg->executeAsChildAlg();
      }
    }

    double OptimizeLatticeForCellType::optLatticeSum(std::string inname, std::string cell_type, std::vector<double> & Params, int edge)
    {
        size_t nData = 15500;
        double* out = new double[nData];
        optLattice(inname, cell_type, Params, edge, out);
        double ChiSqTot=0;
        for( size_t i = 0; i<nData; i++ )ChiSqTot += out[i];
        delete[] out;
    	return ChiSqTot;
    }
   //-----------------------------------------------------------------------------------------
    /**
     * Calls Gaussian1D as a child algorithm to fit the offset peak in a spectrum
     * @param mosaic
     * @param rcrystallite
     * @param inname
     * @param corrOption
     * @param pointOption
     * @param tofParams
     * @return
     */
    void OptimizeLatticeForCellType::optLattice(std::string inname, std::string cell_type, std::vector<double> & params, int edge, double *out)
    {
      PeaksWorkspace_sptr ws = boost::dynamic_pointer_cast<PeaksWorkspace>
           (AnalysisDataService::Instance().retrieve(inname));
      const std::vector<Peak> &peaks = ws->getPeaks();
      size_t n_peaks = ws->getNumberPeaks();
      std::vector<V3D> q_vector;
      std::vector<V3D> hkl_vector;

      for ( size_t i = 0; i < params.size(); i++ )params[i]=std::abs(params[i]);
      for ( size_t i = 0; i < n_peaks; i++ )
      {
        if (edgePixel(ws, peaks[i].getBankName(), peaks[i].getCol(), peaks[i].getRow(), edge))continue;
        q_vector.push_back(peaks[i].getQSampleFrame());
        hkl_vector.push_back(peaks[i].getHKL());
      }
      std::vector<double> lattice_parameters;
      lattice_parameters.assign (6,0);
      if (cell_type == "Cubic")
      {
        lattice_parameters[0] = params[0];       
        lattice_parameters[1] = params[0];
        lattice_parameters[2] = params[0];      
    
        lattice_parameters[3] = 90;
        lattice_parameters[4] = 90;
        lattice_parameters[5] = 90;
      }
      else if (cell_type == "Tetragonal")
      {
        lattice_parameters[0] = params[0];    
        lattice_parameters[1] = params[0];
        lattice_parameters[2] = params[1];   
    
        lattice_parameters[3] = 90;
        lattice_parameters[4] = 90;
        lattice_parameters[5] = 90;
      }
      else if (cell_type == "Orthorhombic")
      {
        lattice_parameters[0] = params[0];  
        lattice_parameters[1] = params[1];
        lattice_parameters[2] = params[2]; 
    
        lattice_parameters[3] = 90;
        lattice_parameters[4] = 90;
        lattice_parameters[5] = 90;
      }
      else if (cell_type == "Rhombohedral")
      {
        lattice_parameters[0] = params[0];
        lattice_parameters[1] = params[0];
        lattice_parameters[2] = params[0];  
    
        lattice_parameters[3] = params[1];
        lattice_parameters[4] = params[1];
        lattice_parameters[5] = params[1];
      }
      else if (cell_type == "Hexagonal")
      {
        lattice_parameters[0] = params[0]; 
        lattice_parameters[1] = params[0];
        lattice_parameters[2] = params[1];
    
        lattice_parameters[3] = 90;
        lattice_parameters[4] = 90;
        lattice_parameters[5] = 120;
      }
      else if (cell_type == "Monoclinic ( a unique )")
      {
        lattice_parameters[0] = params[0];
        lattice_parameters[1] = params[1];
        lattice_parameters[2] = params[2];
    
        lattice_parameters[3] = params[3];
        lattice_parameters[4] = 90;
        lattice_parameters[5] = 90;
      }
      else if (cell_type == "Monoclinic ( b unique )")
      {
        lattice_parameters[0] = params[0];
        lattice_parameters[1] = params[1];
        lattice_parameters[2] = params[2];
    
        lattice_parameters[3] = 90;
        lattice_parameters[4] = params[3];
        lattice_parameters[5] = 90;
      }
      else if (cell_type == "Monoclinic ( c unique )")
      {
        lattice_parameters[0] = params[0];
        lattice_parameters[1] = params[1];
        lattice_parameters[2] = params[2];
    
        lattice_parameters[3] = 90;
        lattice_parameters[4] = 90;
        lattice_parameters[5] = params[3];
      }
      else if (cell_type == "Triclinic")
      {
        lattice_parameters[0] = params[0];
        lattice_parameters[1] = params[1];
        lattice_parameters[2] = params[2]; 
    
        lattice_parameters[3] = params[3];
        lattice_parameters[4] = params[4];
        lattice_parameters[5] = params[5];
      }
    
      Mantid::API::IAlgorithm_sptr alg = createChildAlgorithm("CalculateUMatrix");
      alg->setPropertyValue("PeaksWorkspace", inname);
      alg->setProperty("a",lattice_parameters[0]);
      alg->setProperty("b",lattice_parameters[1]);
      alg->setProperty("c",lattice_parameters[2]);
      alg->setProperty("alpha",lattice_parameters[3]);
      alg->setProperty("beta",lattice_parameters[4]);
      alg->setProperty("gamma",lattice_parameters[5]);
      alg->executeAsChildAlg();
    
      ws = alg->getProperty("PeaksWorkspace");
      OrientedLattice latt=ws->mutableSample().getOrientedLattice();
      DblMatrix UB = latt.getUB();
      DblMatrix A = A_matrix(lattice_parameters);
      DblMatrix Bc = A;
      Bc.Invert();
      DblMatrix U1_B1 = UB * A;
      OrientedLattice o_lattice;
      o_lattice.setUB( U1_B1 );
      DblMatrix U1 = o_lattice.getU();
      DblMatrix U1_Bc = U1 * Bc;

      for ( size_t i = 0; i < hkl_vector.size(); i++ ) 
      {
         V3D error = U1_Bc * hkl_vector[i] - q_vector[i] / (2.0 * M_PI);
         out[i] = error.norm2();
      }

      return;
    }
    bool OptimizeLatticeForCellType::edgePixel(PeaksWorkspace_sptr ws, std::string bankName, int col, int row, int Edge)
    {
  	  if (bankName.compare("None") == 0) return false;
  	  Geometry::Instrument_const_sptr Iptr = ws->getInstrument();
  	  boost::shared_ptr<const IComponent> parent = Iptr->getComponentByName(bankName);
  	  if (parent->type().compare("RectangularDetector") == 0)
  	  {
  		  boost::shared_ptr<const RectangularDetector> RDet = boost::dynamic_pointer_cast<
  					const RectangularDetector>(parent);

  	      if (col < Edge || col >= (RDet->xpixels()-Edge) || row < Edge || row >= (RDet->ypixels()-Edge)) return true;
  	      else return false;
  	  }
  	  else
  	  {
            std::vector<Geometry::IComponent_const_sptr> children;
            boost::shared_ptr<const Geometry::ICompAssembly> asmb = boost::dynamic_pointer_cast<const Geometry::ICompAssembly>(parent);
            asmb->getChildren(children, false);
            boost::shared_ptr<const Geometry::ICompAssembly> asmb2 = boost::dynamic_pointer_cast<const Geometry::ICompAssembly>(children[0]);
            std::vector<Geometry::IComponent_const_sptr> grandchildren;
            asmb2->getChildren(grandchildren,false);
            int NROWS = static_cast<int>(grandchildren.size());
            int NCOLS = static_cast<int>(children.size());
            // Wish pixels and tubes start at 1 not 0
            if (col-1 < Edge || col-1 >= (NCOLS-Edge) || row-1 < Edge || row-1 >= (NROWS-Edge)) return true;
      	  else return false;
  	  }
  	  return false;
    }
    DblMatrix OptimizeLatticeForCellType::A_matrix( std::vector<double> lattice )
    {
      double degrees_to_radians = M_PI / 180;
      double alpha = lattice[3] * degrees_to_radians;
      double beta  = lattice[4] * degrees_to_radians;
      double gamma = lattice[5] * degrees_to_radians;

      double l1 = lattice[0];
      double l2 = 0;
      double l3 = 0;

      double m1 = lattice[1]*std::cos(gamma);
      double m2 = lattice[1]*std::sin(gamma);
      double m3 = 0;

      double n1 = std::cos(beta);
      double n2 = (std::cos(alpha)- std::cos(beta)*std::cos(gamma)) /
                   std::sin(gamma);
      double n3 = std::sqrt( 1 - n1*n1 - n2*n2 );
      n1 *= lattice[2];
      n2 *= lattice[2];
      n3 *= lattice[2];

      DblMatrix result(3,3);
      result[0][0] = l1;
      result[0][1] = l2;
      result[0][2] = l3;
      result[1][0] = m1;
      result[1][1] = m2;
      result[1][2] = m3;
      result[2][0] = n1;
      result[2][1] = n2;
      result[2][2] = n3;

      return result;
    }
    /**

      @param  npeaks       Number of peaks
      @param  inname       Name of workspace containing peaks
      @param  cell_type    type from dropdown list
      @param  Params       optimized cell parameters
      @param  edgePixels   pixels to ignore at edge of detectors
      @param  sigabc       errors of optimized parameters
      @param  chisq        chisq from optimization
    */
    void OptimizeLatticeForCellType::Calculate_Errors(size_t npeaks, std::string inname, std::string cell_type,
    		                          std::vector<double> & Params, int edgePixels,
                                      std::vector<double> & sigabc, double chisq)
    {
      double result = chisq;
      for( size_t i=0;i<sigabc.size();i++)
          sigabc[i]=0.0;

      size_t nDOF= 3*(npeaks-3);
      int MAX_STEPS = 10;        // evaluate approximation using deltas
                                       // ranging over 10 orders of magnitudue
      double START_DELTA = 1.0e-2;     // start with change of 1%
      std::vector<double> approx;   // save list of approximations

      double a_save;
      double delta;
      for ( size_t k = 0; k < Params.size(); k++ )
      {
    	std::vector<double> a = Params;
        double diff = 0.0;

        a_save = a[k];

        if ( a_save < 1.0e-8 )             // if parameter essentially 0, use
          delta = 1.0e-8;                  // a "small" step
        else
          delta = START_DELTA * a_save;

        for ( int count = 0; count < MAX_STEPS; count++ )
        {
          a[k] = a_save + delta;
          double chi_3 = optLatticeSum(inname, cell_type, a, edgePixels);

          a[k] = a_save - delta;
          double chi_1 = optLatticeSum(inname, cell_type, a, edgePixels);

          a[k] = a_save;
          double chi_2 = optLatticeSum(inname, cell_type, a, edgePixels);

          diff = chi_1-2*chi_2+chi_3;
          if ( diff > 0 )
          {
            approx.push_back(std::abs(delta) *
                               std::sqrt(2.0/std::abs(diff)));
          }
          delta = delta / 10;
        }
        if ( approx.size() == 0 )
        	sigabc[k] = Mantid::EMPTY_DBL();    // no reasonable value

        else if ( approx.size() == 1 )
        	sigabc[k] = approx[0];                   // only one possible value

        else                                        // use one with smallest diff
        {
          double min_diff = Mantid::EMPTY_DBL();
          for ( size_t i = 0; i < approx.size(); i++ )
          {
            diff = std::abs( (approx[i+1]-approx[i])/approx[i] );
            if ( diff < min_diff )
            {
              sigabc[k] = approx[i+1];
              min_diff = diff;
            }
          }
        }
      }


      delta = result /(double)nDOF;

      for( size_t i = 0; i < std::min<size_t>(7,sigabc.size()); i++ )
                sigabc[i] = sqrt( delta) * sigabc[i];

    }
  } // namespace Algorithm
} // namespace Mantid

#include <stdexcept>
#include "MantidDataObjects/EventList.h"
#include "MantidKernel/Exception.h"

using std::ostream;
using std::runtime_error;
using std::size_t;
using std::vector;

namespace Mantid
{
namespace DataObjects
{
  using Kernel::Exception::NotImplementedError;

  //==========================================================================
  /// --------------------- TofEvent stuff ----------------------------------
  //==========================================================================
  TofEvent::TofEvent(const double time_of_flight, const size_t frameid)
  {
	  this->time_of_flight = time_of_flight;
	  this->frame_index = frameid;
  }

  TofEvent::TofEvent(const TofEvent& rhs)
  {
    this->time_of_flight = rhs.time_of_flight;
    this->frame_index = rhs.frame_index;
  }

  TofEvent::TofEvent()
  {
    this->time_of_flight = 0;
    this->frame_index = 0;
  }

  TofEvent::~TofEvent()
  {
  }

  TofEvent& TofEvent::operator=(const TofEvent& rhs) {
	  this->time_of_flight = rhs.time_of_flight;
	  this->frame_index = rhs.frame_index;
	  return *this;
  }

  double TofEvent::tof()
  {
	  return this->time_of_flight;
  }

  size_t TofEvent::frame()
  {
	  return this->frame_index;
  }

  ostream& operator<<(ostream &os, const TofEvent &event)
  {
    os << event.time_of_flight << "," << event.frame_index;
    return os;
  }

  /// --------------------- TofEvent Comparators ----------------------------------
  /** Compare two events' TOF, return true if e1 should be before e2. */
  bool compareEventTof(TofEvent e1, TofEvent e2)
  {
    return (e1.tof() < e2.tof());
  }

  /** Compare two events' FRAME id, return true if e1 should be before e2. */
  bool compareEventFrame(TofEvent e1, TofEvent e2)
  {
    return (e1.frame() < e2.frame());
  }




  //==========================================================================
  // ---------------------- EventList stuff ----------------------------------
  //==========================================================================

  // --- Constructors -------------------------------------------------------------------

  EventList::EventList()
  {
    this->order = UNSORTED;
    this->emptyCache();
  }

  EventList::EventList(const EventList& rhs)
  {
    //Call the copy operator to do the job,
    this->operator=(rhs);
  }

  EventList::EventList(const vector<TofEvent> &events)
  {
    this->events.assign(events.begin(), events.end());
    this->order = UNSORTED;
    this->emptyCache();
  }

  EventList::~EventList()
  {
  }

  // --- Operators -------------------------------------------------------------------

  EventList& EventList::operator=(const EventList& rhs)
  {
    //Copy all data from the rhs.
    this->events.assign(rhs.events.begin(), rhs.events.end());
    this->refX = rhs.refX;
    this->refY = rhs.refY;
    this->refE = rhs.refE;
    this->order = rhs.order;
    return *this;
  }

  EventList& EventList::operator+=(const TofEvent &event)
  {
    this->events.push_back(event);
    this->order = UNSORTED;
    this->emptyCacheData();
    return *this;
  }

  EventList& EventList::operator+=(const std::vector<TofEvent> & more_events)
  {
    this->events.insert(this->events.end(), more_events.begin(), more_events.end());
    this->order = UNSORTED;
    this->emptyCacheData();
    return *this;
  }

  EventList& EventList::operator+=(EventList& more_events)
  {
    vector<TofEvent> rel = more_events.getEvents();
    this->events.insert(this->events.end(), rel.begin(), rel.end());
    this->order = UNSORTED;
    this->emptyCacheData();
    return *this;
  }

  // --- Handling the event list -------------------------------------------------------------------
  std::vector<TofEvent>& EventList::getEvents()
  {
    return this->events;
  }

  void EventList::clear()
  {
    this->events.clear();
    this->emptyCacheData();
  }

  // --- Sorting functions -------------------------------------------------------------------
  void EventList::sort(const EventSortType order) const
  {
    if (order == UNSORTED)
    {
      return; // don't bother doing anything. Why did you ask to unsort?
    }
    else if (order == TOF_SORT)
    {
      this->sortTof();
    }
    else if (order == FRAME_SORT)
    {
      this->sortFrame();
    }
    else
    {
      throw runtime_error("Invalid sort type in EventList::sort(EventSortType)");
    }
  }

  void EventList::sortTof() const
  {
    if (this->order == TOF_SORT)
    {
      return; // nothing to do
    }
    //Perform sort.
    std::sort(events.begin(), events.end(), compareEventTof);
    //Save the order to avoid unnecessary re-sorting.
    this->order = TOF_SORT;
  }

  void EventList::sortFrame() const
  {
    if (this->order == FRAME_SORT)
    {
      return; // nothing to do
    }
    //Perform sort.
    std::sort(events.begin(), events.end(), compareEventFrame);
    //Save the order to avoid unnecessary re-sorting.
    this->order = FRAME_SORT;
  }


  size_t EventList::getNumberEvents() const
  {
    return this->events.size();
  }

  // --- Setting the Histrogram X axis, without recalculating the cache -------------------------
  void EventList::setX(const RCtype::ptr_type& X, Unit* set_xUnit)
  {
    this->emptyCache();
    this->refX = X;
    if (!(set_xUnit == NULL))
      this->xUnit = set_xUnit;
  }

  void EventList::setX(const RCtype& X, Unit* set_xUnit)
  {
    this->emptyCache();
    this->refX = X;
    if (!(set_xUnit == NULL))
      this->xUnit = set_xUnit;
  }

  void EventList::setX(const StorageType& X, Unit* set_xUnit)
  {
    this->emptyCache();
    this->refX.access()=X;
    if (!(set_xUnit == NULL))
      this->xUnit = set_xUnit;
  }


  // --- Return Data Vectors -------------------------------------------------------------
  // Note: these will only be called from a const class; e.g
  // const EventList el;
  // el.dataX(); <<<<< this works
  // EventList el2;
  // el2.dataX(); <<<<< this throws an error.

  const EventList::StorageType& EventList::dataX() const
  {
    return *(this->refX);
  }

  const EventList::StorageType& EventList::dataY() const
  {
//    if (refX->size() <= 0)
//      throw std::runtime_error("Histogram X not set!");
    //this->refY.access().push_back(1234.5678);
    this->generateHistogram();
    return *(this->refY);
  }

  const EventList::StorageType& EventList::dataE() const
  {
//    if (refX->size() <= 0)
//      throw std::runtime_error("Histogram X not set!");
    //this->generateHistogram();
    return *(this->refE);
  }

  Kernel::cow_ptr<MantidVec> EventList::getRefX() const
  {
    return refX;
  }

  // --- Non-const return Data Vectors throw exceptions! -------------------------------------------------------------
  EventList::StorageType& EventList::dataX()
  {
    //return *(this->refX);
    throw NotImplementedError("EventList::dataX cannot return a non-const array: you can't modify the histogrammed data in an EventWorkspace!");
  }
  EventList::StorageType& EventList::dataY()
  {
    throw NotImplementedError("EventList::dataY cannot return a non-const array: you can't modify the histogrammed data in an EventWorkspace!");
  }
  EventList::StorageType& EventList::dataE()
  {
    throw NotImplementedError("EventList::dataE cannot return a non-const array: you can't modify the histogrammed data in an EventWorkspace!");
  }


  // --- Histogram functions -------------------------------------------------
  void EventList::emptyCache() const
  {
    this->refX.access().clear();
    this->refY.access().clear();
    this->refE.access().clear();
  }

  void EventList::emptyCacheData()
  {
    this->refY.access().clear();
    this->refE.access().clear();
  }


  void EventList::generateHistogram() const
  {
    StorageType &Y = refY.access();
    StorageType &X = refX.access();

    //For slight speed=up.
    size_t x_size = refX->size();

    if (x_size <= 1)
    {
      //throw runtime_error("EventList::generateHistogram() called without the X axis set previously.");

      //By default, if no histogram bins are set, simply sum up all events!
      // This is equivalent to a single bin from -inf to +inf.
      Y.resize(1, 0);
      //Set the single bin to the total # of events.
      Y[0] = this->events.size();
      //And we're done! That was easy.
      return;
    }

    //TODO: Should we have a smarter check for this?
    if (Y.size() != x_size)
    {
      //Need to redo the histogram.
      //Sort the events by tof
      this->sortTof();
      //Clear the Y data, assign all to 0.
      Y.resize(x_size, 0);

      //Do we even have any events to do?
      if (this->events.size() > 0)
      {
        //Iterate through all events (sorted by tof)
        std::vector<TofEvent>::iterator itev = this->events.begin();

        //Find the first bin
        size_t bin=0;
        double tof = itev->tof();
        while (bin < x_size-1)
        {
          //Within range?
          if ((tof >= X[bin]) && (tof < X[bin+1]))
          {
            Y[bin]++;
            break;
          }
          ++bin;
        }
        //Go to the next event, we've already binned this first one.
        ++itev;

        //Keep going through all the events
        while ((itev != this->events.end()) && (bin < x_size-1))
        {
          tof = itev->tof();
          while (bin < x_size-1)
          {
            //Within range?
            if ((tof >= X[bin]) && (tof < X[bin+1]))
            {
              Y[bin]++;
              break;
            }
            ++bin;
          }
          ++itev;
        }
      } // end if (there are any events to histogram)
    }// end if (we need to re-histogram)


  }

} /// namespace DataObjects
} /// namespace Mantid

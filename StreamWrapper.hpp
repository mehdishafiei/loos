/*
  StreamWrapper
  (c) 2008 Tod D. Romo

  Grossfield Lab
  Department of Biochemistry and Biophysics
  University of Rochester Medical School

*/


#if !defined(STREAMWRAPPER_HPP)
#define STREAMWRAPPER_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>

#include <boost/utility.hpp>

using namespace std;

//! Simple wrapper class for caching stream pointers
/** This class was written primarily for use with the DCD classes
 *  where we want to have a cached stream that we may read from (or
 *  write to) at various times in the future.  Access to the
 *  underlying fstream pointer is through the operator() functor.
 *
 *  The basic idea here is that you pass the class either a string or
 *  a char array and that will be opened into a new stream (reading by
 *  default).  The fstream pointer will be stored and when the wrapper
 *  object is destroyed, the stream is released & deleted.  If you
 *  pass the wrapper an fstream, however, the internal pointer is
 *  initialized to point to that stream and when the wrapper object is
 *  destroyed, the stream is left alone.
 */
class StreamWrapper : public boost::noncopyable {
public:
  StreamWrapper() : new_stream(false), stream(0) { }

  //! Sets the internal stream pointer to fs
  StreamWrapper(fstream& fs) : new_stream(false), stream(&fs) { }

  //! Opens a new stream with file named 's'
  StreamWrapper(const string& s, const ios_base::openmode mode = ios_base::in | ios_base::binary) : new_stream(true) {
    stream = new fstream(s.c_str(), mode);
    if (!stream)
      throw(runtime_error("Cannot open file " + s));
  }

  //! Opens a new stream with file named 's'
  StreamWrapper(const char* const s, const ios_base::openmode mode = ios_base::in | ios_base::binary) : new_stream(true) {
    stream = new fstream(s, mode);
    if (!stream)
      throw(runtime_error("Cannot open file " + string(s)));
  }

  //! Sets the internal stream to the passed fstream.
  void setStream(fstream& fs) {
    if (new_stream)
      delete stream;
    new_stream = false;
    stream = &fs;
  }

  //! Returns the internal fstream pointer
  fstream* operator()(void) {
    if (stream == 0)
      throw(logic_error("Attempting to access an unset stream"));
    return(stream);
  }

  //! Returns true if the internal stream pointer is unset
  bool isUnset(void) const { return(stream == 0); }

  //! Checks to see if the stream pointer is set and throws an exception if not.
  void checkSet(void) const {
    if (stream == 0)
      throw(logic_error("Attempting to use an unset stream"));
  }
    
  ~StreamWrapper() { if (new_stream) delete stream; }


private:
  bool new_stream;
  fstream* stream;
};


#endif

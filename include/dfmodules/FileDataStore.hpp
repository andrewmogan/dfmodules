/**
 * @file FileDataStore.hpp
 *
 * This is the interface for storing and retrieving data from
 * file-based storage systems.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

// 09-Sep-2020, KAB: the initial version of this class was based on the
// Queue interface from the appfwk repo.

#ifndef DFMODULES_INCLUDE_DFMODULES_FILEDATASTORE_HPP_
#define DFMODULES_INCLUDE_DFMODULES_FILEDATASTORE_HPP_

#include "daqdataformats/Types.hpp"
#include "dfmodules/DataStore.hpp"

#include <string>
#include <vector>

namespace dunedaq::dfmodules {

class FileDataStore : public DataStore
{
public:
  /**
   * @brief FileDataStore Constructor
   * @param name Name of the DataStore instance
   */
  FileDataStore(const std::string& name,
                std::shared_ptr<dunedaq::appfwk::ConfigurationManager> cfgmgr,
                const std::string& identifier)
    : DataStore(name, cfgmgr, identifier)
  {
  }

  /**
   * @brief Read a TriggerRecord from a file in the DataStore
   * @param file_name File to read
   * @param trigger_number Trigger Number to read (or s_invalid_trigger_number for implementation-defined "next"
   * TriggerRecord)
   * @param sequence_number Sequence Number to read (or s_invalid_sequence_number for implementation-defined "next"
   * TriggerRecord)
   * @return std::optional containing the TriggerRecord, if one matched the request
   */
  virtual std::optional<daqdataformats::TriggerRecord> read_trigger_record_from_file(
    std::string const& file_name,
    daqdataformats::trigger_number_t trigger_number = daqdataformats::TypeDefaults::s_invalid_trigger_number,
    daqdataformats::sequence_number_t sequence_number = daqdataformats::TypeDefaults::s_invalid_sequence_number)
  {
    set_file_name_for_reading(file_name);
    return read_trigger_record(trigger_number, sequence_number);
  }

  /**
   * @brief Read a TimeSlice from a file in the DataStore
   * @param file_name File to read
   * @param timeslice_number TimeSlice Number to read (or s_invalid_timeslice_number for implementation-defined "next"
   * TimeSlice)
   * @return std::optional containing the TimeSlice, if one matched the request
   */
  virtual std::optional<daqdataformats::TimeSlice> read_time_slice_from_file(
    std::string const& file_name,
    daqdataformats::timeslice_number_t timeslice_number = daqdataformats::TypeDefaults::s_invalid_timeslice_number)
  {
    set_file_name_for_reading(file_name);
    return read_time_slice(timeslice_number);
  }

  virtual std::vector<std::string> get_available_files(
    daqdataformats::run_number_t run_number = daqdataformats::TypeDefaults::s_invalid_run_number,
    bool restrict_by_identifier = false) = 0;

protected:
  virtual void set_file_name_for_reading(std::string const& file_name) = 0;
};

} // namespce dunedaq::dfmodules

#endif // DFMODULES_INCLUDE_DFMODULES_FILEDATASTORE_HPP_

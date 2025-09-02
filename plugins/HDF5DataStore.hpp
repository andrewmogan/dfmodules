/**
 * @file HDF5DataStore.hpp
 *
 * An implementation of the DataStore interface that uses the
 * highFive library to create objects in HDF5 Groups and datasets
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_HDF5DATASTORE_HPP_
#define DFMODULES_PLUGINS_HDF5DATASTORE_HPP_

#include "HDF5FileUtils.hpp"
#include "dfmodules/FileDataStore.hpp"
#include "dfmodules/opmon/DataStore.pb.h"

#include "hdf5libs/HDF5RawDataFile.hpp"

#include "appmodel/DataStoreConf.hpp"
#include "appmodel/FilenameParams.hpp"
#include "confmodel/DetectorConfig.hpp"
#include "confmodel/Session.hpp"

#include "appfwk/DAQModule.hpp"
#include "logging/Logging.hpp" // NOTE: if ISSUES ARE DECLARED BEFORE include logging/Logging.hpp, TLOG_DEBUG<<issue wont work.

#include "boost/date_time/posix_time/posix_time.hpp"
#include "boost/lexical_cast.hpp"

#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <sys/statvfs.h>
#include <utility>
#include <vector>

namespace dunedaq {

// Disable coverage checking LCOV_EXCL_START
/**
 * @brief A ERS Issue to report an HDF5 exception
 */
ERS_DECLARE_ISSUE_BASE(dfmodules,
                       InvalidOperationMode,
                       appfwk::GeneralDAQModuleIssue,
                       "Selected operation mode \"" << selected_operation
                                                    << "\" is NOT supported. Please update the configuration file.",
                       ((std::string)name),
                       ((std::string)selected_operation))

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       FileOperationProblem,
                       appfwk::GeneralDAQModuleIssue,
                       "A problem was encountered when opening or closing file \"" << filename << "\"",
                       ((std::string)name),
                       ((std::string)filename))

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       InvalidFileHandle,
                       appfwk::GeneralDAQModuleIssue,
                       "Invalid or null file handle encountered",
                       ((std::string)name),
                       ERS_EMPTY)

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       InvalidHDF5Dataset,
                       appfwk::GeneralDAQModuleIssue,
                       "The HDF5 Dataset associated with name \"" << data_set << "\" is invalid. (file = " << filename
                                                                  << ")",
                       ((std::string)name),
                       ((std::string)data_set)((std::string)filename))

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       InvalidOutputPath,
                       appfwk::GeneralDAQModuleIssue,
                       "The specified output destination, \"" << output_path
                                                              << "\", is not a valid file system path on this server.",
                       ((std::string)name),
                       ((std::string)output_path))

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       InsufficientDiskSpace,
                       appfwk::GeneralDAQModuleIssue,
                       "There is insufficient free space on the disk associated with output file path \""
                         << path << "\". There are " << free_bytes << " bytes free, and the "
                         << "required minimum is " << needed_bytes << " bytes based on " << criteria << ".",
                       ((std::string)name),
                       ((std::string)path)((size_t)free_bytes)((size_t)needed_bytes)((std::string)criteria))

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       EmptyDataBlockList,
                       appfwk::GeneralDAQModuleIssue,
                       "There was a request to write out a list of data blocks, but the list was empty. "
                         << "Ignoring this request",
                       ((std::string)name),
                       ERS_EMPTY)

// Re-enable coverage checking LCOV_EXCL_STOP
namespace dfmodules {

/**
 * @brief HDF5DataStore creates an HDF5 instance
 * of the DataStore class
 */
class HDF5DataStore : public FileDataStore
{

public:
  enum
  {
    TLVL_BASIC = 2,
    TLVL_FILE_SIZE = 5
  };

  /**
   * @brief HDF5DataStore Constructor
   * @param name, path, filename, operationMode
   *
   */
  HDF5DataStore(std::string const& name,
                std::shared_ptr<appfwk::ConfigurationManager> mcfg,
                std::string const& writer_name);

  /**
   * @brief HDF5DataStore write()
   * Method used to write constant data
   * into HDF5 format. Operational mode
   * defined in the configuration file.
   *
   */
  void write(const daqdataformats::TriggerRecord& tr) override;

  /**
   * @brief HDF5DataStore write()
   * Method used to write constant data
   * into HDF5 format. Operational mode
   * defined in the configuration file.
   *
   */
  void write(const daqdataformats::TimeSlice& ts) override;

  /**
   * @brief Read a TriggerRecord from the DataStore
   * @param trigger_number Trigger Number to read (or s_invalid_trigger_number for implementation-defined "next"
   * TriggerRecord)
   * @param sequence_number Sequence Number to read (or s_invalid_sequence_number for implementation-defined "next"
   * TriggerRecord)
   * @return std::optional containing the TriggerRecord, if one matched the request
   */
  std::optional<daqdataformats::TriggerRecord> read_trigger_record(
    daqdataformats::trigger_number_t trigger_number = daqdataformats::TypeDefaults::s_invalid_trigger_number,
    daqdataformats::sequence_number_t sequence_number = daqdataformats::TypeDefaults::s_invalid_sequence_number) override;

  /**
   * @brief Read a TimeSlice from the DataStore
   * @param timeslice_number TimeSlice Number to read (or s_invalid_timeslice_number for implementation-defined "next"
   * TimeSlice)
   * @return std::optional containing the TimeSlice, if one matched the request
   */
  std::optional<daqdataformats::TimeSlice> read_time_slice(
    daqdataformats::timeslice_number_t timeslice_number = daqdataformats::TypeDefaults::s_invalid_timeslice_number) override;

  /**
   * @brief Informs the HDF5DataStore that writes or reads of records
   * associated with the specified run number will soon be requested.
   * This allows the DataStore to test that the output file path is valid
   * and any other checks that are useful in advance of the first data
   * blocks being written or read.
   *
   * This method may throw an exception if it finds a problem.
   */
  void prepare_for_run(daqdataformats::run_number_t run_number, bool run_is_for_test_purposes) override;

  /**
   * @brief Informs the HD5DataStore that writes or reads of records
   * associated with the specified run number have finished, for now.
   * This allows the DataStore to close open files and do any other
   * cleanup or shutdown operations that are useful once the writes or
   * reads for a given run number have finished.
   */
  void finish_with_run(daqdataformats::run_number_t /*run_number*/) override;

  std::vector<std::string> get_available_files(daqdataformats::run_number_t run_number, bool restrict_by_identifier) override;

  void set_file_name_for_reading(std::string const& file_name) override;

protected:
  void generate_opmon_data() override;

private:
  HDF5DataStore(const HDF5DataStore&) = delete;
  HDF5DataStore& operator=(const HDF5DataStore&) = delete;
  HDF5DataStore(HDF5DataStore&&) = delete;
  HDF5DataStore& operator=(HDF5DataStore&&) = delete;

  std::unique_ptr<hdf5libs::HDF5RawDataFile> m_file_handle;
  const appmodel::HDF5FileLayoutParams* m_file_layout_params;
  std::string m_basic_name_of_open_file;
  unsigned m_open_flags_of_open_file;
  daqdataformats::run_number_t m_run_number;
  bool m_run_is_for_test_purposes;
  const confmodel::Session* m_session;
  std::string m_operational_environment;
  std::string m_offline_data_stream;
  std::string m_writer_identifier;

  // Total number of generated files
  std::atomic<size_t> m_file_index;

  // Size of data being written, excluding metadata
  std::atomic<size_t> m_recorded_size;

  // Theoretical, "uncompressed" size of data being written, excluding metadata
  std::atomic<size_t> m_uncompressed_raw_data_size;

  // Used for tracking the "delta" of the current write
  std::atomic<size_t> m_previous_file_size = 0;

  // Total size of the file, including raw data, metadata, and free space
  std::atomic<size_t> m_total_file_size;

  // Record number for the record that is currently being written out
  // This is only useful for long-readout windows, in which there may
  // be multiple calls to write()
  size_t m_current_record_number;
  // Used when reading TriggerRecords that are broken into sequences
  daqdataformats::sequence_number_t m_current_sequence_number{ 0 };

  // incremental written data
  std::atomic<uint64_t> m_new_bytes;
  std::atomic<uint64_t> m_new_objects;

  // Configuration
  const appmodel::DataStoreConf* m_config_params;
  std::string m_operation_mode;
  std::string m_path;
  size_t m_max_file_size;
  bool m_disable_unique_suffix;
  float m_free_space_safety_factor_for_write;
  unsigned m_compression_level;

  // std::unique_ptr<HDF5KeyTranslator> m_key_translator_ptr;

  /**
   * @brief Translates the specified input parameters into the appropriate filename.
   */
  std::string get_file_name(daqdataformats::run_number_t run_number);

  bool increment_file_index_if_needed(size_t size_of_next_write);

  void open_file_if_needed(const std::string& file_name, unsigned open_flags = HighFive::File::ReadOnly);

  size_t get_free_space(const std::string& the_path);
};

} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_PLUGINS_HDF5DATASTORE_HPP_

// Local Variables:
// c-basic-offset: 2
// End:

/* -*- mode: C++ -*-
 *
 *  Copyright (C) 2007 Austin Robot Technology, Yaxin Liu, Patrick Beeson
 *  Copyright (C) 2009, 2010 Austin Robot Technology, Jack O'Quin
 *
 *  License: Modified BSD Software License Agreement
 *
 *  $Id$
 */

/** \file
 *
 *  Velodyne HDL-64E 3D LIDAR data accessor base class.
 *
 *  Base class for unpacking raw Velodyne LIDAR packets into various
 *  useful formats.
 *
 *  \author Yaxin Liu
 *  \author Patrick Beeson
 *  \author Jack O'Quin
 */

#ifndef __VELODYNE_DATA_BASE_H
#define __VELODYNE_DATA_BASE_H

#include <errno.h>
#include <stdint.h>
#include <string>
#include <boost/format.hpp>

#include <ros/ros.h>
#include <velodyne_common/RawScan.h>
#include <velodyne_msgs/VelodyneScan.h>

namespace Velodyne
{

  /**
   * Raw Velodyne packet constants and structures.
   */
  static const int SIZE_BLOCK = 100;
  static const int RAW_SCAN_SIZE = 3;
  static const int SCANS_PER_BLOCK = 32;
  static const int BLOCK_DATA_SIZE = (SCANS_PER_BLOCK * RAW_SCAN_SIZE);

  static const float ROTATION_RESOLUTION = 0.01f; /**< degrees */
  static const float ROTATION_MAX_UNITS = 36000; /**< hundredths of degrees */

  /** According to Bruce Hall DISTANCE_MAX is 65.0, but we noticed
   *  valid packets with readings up to 130.0. */
  static const float DISTANCE_MAX = 130.0f;        /**< meters */
  static const float DISTANCE_RESOLUTION = 0.002f; /**< meters */
  static const float DISTANCE_MAX_UNITS = (DISTANCE_MAX
                                           / DISTANCE_RESOLUTION + 1.0);
  static const uint16_t UPPER_BANK = 0xeeff;
  static const uint16_t LOWER_BANK = 0xddff;

  /** \brief Raw Velodyne data block.
   *
   *  Each block contains data from either the upper or lower laser
   *  bank.  The device returns three times as many upper bank blocks.
   *
   *  use stdint.h types, so things work with both 64 and 32-bit machines
   */
  typedef struct raw_block
  {
    uint16_t header;        ///< UPPER_BANK or LOWER_BANK
    uint16_t rotation;      ///< 0-35999, divide by 100 to get degrees
    uint8_t  data[BLOCK_DATA_SIZE];
  } raw_block_t;

  /** used for unpacking the first two data bytes in a block
   *
   *  They are packed into the actual data stream misaligned.  I doubt
   *  this works on big endian machines.
   */
  union two_bytes
  {
    uint16_t uint;
    uint8_t  bytes[2];
  };

  static const int PACKET_SIZE = 1206;
  static const int BLOCKS_PER_PACKET = 12;
  static const int PACKET_STATUS_SIZE = 4;
  static const int SCANS_PER_PACKET = (SCANS_PER_BLOCK * BLOCKS_PER_PACKET);
  static const int PACKETS_PER_REV = 260;
  static const int SCANS_PER_REV = (SCANS_PER_PACKET * PACKETS_PER_REV);

  /** \brief Raw Velodyne packet.
   *
   *  revolution is described in the device manual as incrementing
   *    (mod 65536) for each physical turn of the device.  Our device
   *    seems to alternate between two different values every third
   *    packet.  One value increases, the other decreases.
   *
   *  \todo figure out if revolution is only present for one of the
   *  two types of status fields
   *
   *  status has either a temperature encoding or the microcode level
   */
  typedef struct raw_packet
  {
    raw_block_t blocks[BLOCKS_PER_PACKET];
    uint16_t revolution;
    uint8_t status[PACKET_STATUS_SIZE]; 
  } raw_packet_t;

  /** \brief Correction angles for a specific HDL-64E device. */
  struct correction_angles
  {
    float rotational;
    float vertical;
    float offset1, offset2, offset3;
    float horzCorr, vertCorr;
    int   enabled;
  };

  /** \brief Base Velodyne data class -- not used directly. */
  class Data
  {
  public:
    Data(std::string ofile="", std::string anglesFile="");

    virtual ~Data() {}

    /** \brief Get ROS parameters.
     *
     *  ROS parameter settings override constructor options
     *
     * \returns 0, if successful;
     *          errno value, for failure
     */
    virtual int getParams(void);

    /** \brief Print data to human-readable file.
     *
     * \returns 0, if successful;
     *          errno value, for failure
     */
    virtual int print(void) = 0;

    /** \brief handle ROS topic message
     *
     *  This is the main entry point for handling ROS messages from
     *  the "velodyne/packets" topic, published by the driver nodelet.
     *  This is \b not a virtual function; derived classes may replace
     *  processPacket(), instead.
     *
     *  By default, the driver publishes packets for a complete
     *  rotation of the device in each message, but that is not
     *  guaranteed.
     */
    void processScan(const velodyne_msgs::VelodyneScan::ConstPtr &scanMsg);

    /** \brief Process a Velodyne packet.
     *
     * \param pkt -> VelodynePacket message
     */
    virtual void processPacket(const velodyne_msgs::VelodynePacket *pkt,
                               const std::string &frame_id) = 0;

    /** \brief Set up for data processing.
     *
     *  Perform initializations needed before data processing can
     *  begin:
     *
     *    - open output file (if any)
     *    - read device-specific correction angles
     *
     *  \returns 0 if successful;
     *           errno value for failure
     */
    virtual int setup(void);

    /** \brief Shut down data processing. */
    virtual void shutdown(void)
    {
      ros::shutdown();                  // break out of main ros::spin()
      uninitialized_ = true;
    }

  protected:

    /** configuration parameters */
    std::string ofile_;                 ///< output file name for print()
    std::string anglesFile_;            ///< correction angles file name

    /** runtime state */
    FILE *ofp_;                         ///< output file descriptor
    bool uninitialized_;                ///< false after successful setup()

    /** latest raw scan message received */
    velodyne_msgs::VelodyneScan::ConstPtr rawScan_;

    /** correction angles indexed by laser within bank
     *
     * \todo combine them into a single array, lower followed by upper
     */
    correction_angles lower_[SCANS_PER_BLOCK];
    correction_angles upper_[SCANS_PER_BLOCK];
  };

} // velodyne namespace

#endif // __VELODYNE_DATA_BASE_H

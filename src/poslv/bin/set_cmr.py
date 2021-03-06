#!/usr/bin/env python

################################################################################
# Copyright (C) 2013 by Jerome Maye                                            #
# jerome.maye@gmail.com                                                        #
#                                                                              #
# This program is free software; you can redistribute it and/or modify         #
# it under the terms of the Lesser GNU General Public License as published by  #
# the Free Software Foundation; either version 3 of the License, or            #
# (at your option) any later version.                                          #
#                                                                              #
# This program is distributed in the hope that it will be useful,              #
# but WITHOUT ANY WARRANTY; without even the implied warranty of               #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                #
# Lesser GNU General Public License for more details.                          #
#                                                                              #
# You should have received a copy of the Lesser GNU General Public License     #
# along with this program. If not, see <http://www.gnu.org/licenses/>.         #
################################################################################

import sys, roslib, rospy
from poslv.srv import *

def setMode(mode):
  rospy.wait_for_service("/poslv/set_dgps")
  try:
    request = rospy.ServiceProxy("/poslv/set_dgps", SetDGPS)
    response = request(mode)
    if response.response:
      print "Mode set to: %s" %(mode)
    else:
      print "Failed to set mode to: %s" %(mode)
      print "Reason: %s" % response.message
  except rospy.ServiceException, exception:
    print "SetDGPS request failed: %s" % exception

if __name__ == "__main__":
  mode = "cmr"
  setMode(mode)

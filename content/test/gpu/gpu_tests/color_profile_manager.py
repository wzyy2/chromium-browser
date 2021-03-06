# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import atexit
import sys

has_forced_srgb = False

# Force all displays to use an sRGB color profile until atexit.
def ForceUntilExitSRGB():
  global has_forced_srgb
  if not sys.platform.startswith('darwin'):
    return
  if has_forced_srgb:
    return
  has_forced_srgb = True

  from gpu_tests import color_profile_manager_mac
  # Record the current color profiles.
  display_profile_url_map = \
      color_profile_manager_mac.GetDisplaysToProfileURLMap()
  # Force to sRGB.
  for display_id in display_profile_url_map:
    color_profile_manager_mac.SetDisplayCustomProfile(
        display_id, color_profile_manager_mac.GetSRGBProfileURL())
  # Register an atexit handler to restore the previous color profiles.
  def Restore():
    for display_id in display_profile_url_map:
      color_profile_manager_mac.SetDisplayCustomProfile(
          display_id, display_profile_url_map[display_id])
  atexit.register(Restore)

#
#  suggest_Si.cpp
#
#  This is an example to run ALM in the suggest mode.
#

import alm
import numpy as np

alm.alm_new()
lavec = [[20.406, 0, 0],
         [0, 20.406, 0],
         [0, 0, 20.406]]
xcoord = [[ 0.0000000000000000, 0.0000000000000000, 0.0000000000000000],
          [ 0.0000000000000000, 0.0000000000000000, 0.5000000000000000],
          [ 0.0000000000000000, 0.2500000000000000, 0.2500000000000000],
          [ 0.0000000000000000, 0.2500000000000000, 0.7500000000000000],
          [ 0.0000000000000000, 0.5000000000000000, 0.0000000000000000],
          [ 0.0000000000000000, 0.5000000000000000, 0.5000000000000000],
          [ 0.0000000000000000, 0.7500000000000000, 0.2500000000000000],
          [ 0.0000000000000000, 0.7500000000000000, 0.7500000000000000],
          [ 0.1250000000000000, 0.1250000000000000, 0.1250000000000000],
          [ 0.1250000000000000, 0.1250000000000000, 0.6250000000000000],
          [ 0.1250000000000000, 0.3750000000000000, 0.3750000000000000],
          [ 0.1250000000000000, 0.3750000000000000, 0.8750000000000000],
          [ 0.1250000000000000, 0.6250000000000000, 0.1250000000000000],
          [ 0.1250000000000000, 0.6250000000000000, 0.6250000000000000],
          [ 0.1250000000000000, 0.8750000000000000, 0.3750000000000000],
          [ 0.1250000000000000, 0.8750000000000000, 0.8750000000000000],
          [ 0.2500000000000000, 0.0000000000000000, 0.2500000000000000],
          [ 0.2500000000000000, 0.0000000000000000, 0.7500000000000000],
          [ 0.2500000000000000, 0.2500000000000000, 0.0000000000000000],
          [ 0.2500000000000000, 0.2500000000000000, 0.5000000000000000],
          [ 0.2500000000000000, 0.5000000000000000, 0.2500000000000000],
          [ 0.2500000000000000, 0.5000000000000000, 0.7500000000000000],
          [ 0.2500000000000000, 0.7500000000000000, 0.0000000000000000],
          [ 0.2500000000000000, 0.7500000000000000, 0.5000000000000000],
          [ 0.3750000000000000, 0.1250000000000000, 0.3750000000000000],
          [ 0.3750000000000000, 0.1250000000000000, 0.8750000000000000],
          [ 0.3750000000000000, 0.3750000000000000, 0.1250000000000000],
          [ 0.3750000000000000, 0.3750000000000000, 0.6250000000000000],
          [ 0.3750000000000000, 0.6250000000000000, 0.3750000000000000],
          [ 0.3750000000000000, 0.6250000000000000, 0.8750000000000000],
          [ 0.3750000000000000, 0.8750000000000000, 0.1250000000000000],
          [ 0.3750000000000000, 0.8750000000000000, 0.6250000000000000],
          [ 0.5000000000000000, 0.0000000000000000, 0.0000000000000000],
          [ 0.5000000000000000, 0.0000000000000000, 0.5000000000000000],
          [ 0.5000000000000000, 0.2500000000000000, 0.2500000000000000],
          [ 0.5000000000000000, 0.2500000000000000, 0.7500000000000000],
          [ 0.5000000000000000, 0.5000000000000000, 0.0000000000000000],
          [ 0.5000000000000000, 0.5000000000000000, 0.5000000000000000],
          [ 0.5000000000000000, 0.7500000000000000, 0.2500000000000000],
          [ 0.5000000000000000, 0.7500000000000000, 0.7500000000000000],
          [ 0.6250000000000000, 0.1250000000000000, 0.1250000000000000],
          [ 0.6250000000000000, 0.1250000000000000, 0.6250000000000000],
          [ 0.6250000000000000, 0.3750000000000000, 0.3750000000000000],
          [ 0.6250000000000000, 0.3750000000000000, 0.8750000000000000],
          [ 0.6250000000000000, 0.6250000000000000, 0.1250000000000000],
          [ 0.6250000000000000, 0.6250000000000000, 0.6250000000000000],
          [ 0.6250000000000000, 0.8750000000000000, 0.3750000000000000],
          [ 0.6250000000000000, 0.8750000000000000, 0.8750000000000000],
          [ 0.7500000000000000, 0.0000000000000000, 0.2500000000000000],
          [ 0.7500000000000000, 0.0000000000000000, 0.7500000000000000],
          [ 0.7500000000000000, 0.2500000000000000, 0.0000000000000000],
          [ 0.7500000000000000, 0.2500000000000000, 0.5000000000000000],
          [ 0.7500000000000000, 0.5000000000000000, 0.2500000000000000],
          [ 0.7500000000000000, 0.5000000000000000, 0.7500000000000000],
          [ 0.7500000000000000, 0.7500000000000000, 0.0000000000000000],
          [ 0.7500000000000000, 0.7500000000000000, 0.5000000000000000],
          [ 0.8750000000000000, 0.1250000000000000, 0.3750000000000000],
          [ 0.8750000000000000, 0.1250000000000000, 0.8750000000000000],
          [ 0.8750000000000000, 0.3750000000000000, 0.1250000000000000],
          [ 0.8750000000000000, 0.3750000000000000, 0.6250000000000000],
          [ 0.8750000000000000, 0.6250000000000000, 0.3750000000000000],
          [ 0.8750000000000000, 0.6250000000000000, 0.8750000000000000],
          [ 0.8750000000000000, 0.8750000000000000, 0.1250000000000000],
          [ 0.8750000000000000, 0.8750000000000000, 0.6250000000000000]]

kd = [14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
      14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
      14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
      14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14];
alm.set_cell(lavec, xcoord, kd)
alm.set_norder(2)
alm.set_cutoff_radii([-1, 7.3])
alm.run_suggest()
alm.alm_delete()

#!/usr/bin/env python

# python3 status: compatible

# Stuff for plotting 2D matrices.
#
# In particular, these funcs are for interacting with 3dNetCorr and
# 3dTrackID outputs.

# --------------------------------------------------------------------------
#
# auth: 'PA Taylor'
#
#ver = '0.0' ; date = 'June 1, 2020'
# [PT] matrix-related things, for plotting, stats and other calcs
#
ver = '0.1' ; date = 'June 2, 2020'
# [PT] debugged, testing on netcc and grid files
#
#ver = '0.11' ; date = 'June 2, 2020'
# [PT] fix using roi_intvals as labels
#
ver = '0.12' ; date = 'June 2, 2020'
# [PT] tested in py2 and py3
#    + fixed plotting size stuff with tight layout
#    + put in fix for matplotlib bug for yaxes range
#
# --------------------------------------------------------------------------

import sys, os, copy

import matplotlib.pyplot       as     plt
import matplotlib.colors       as     clr
from   matplotlib              import rcParams
from   mpl_toolkits.axes_grid1 import make_axes_locatable

from   afnipy import afni_base as ab
from   afnipy import afni_util as UTIL
#from afnipy import lib_mat2d_base as LM2D

# ---------------------------------------------------------------------------

class plot_mat2d:

    """A class for plotting a 2D matrix;  need not be square.  

    For cbars, see:
    https://scipy.github.io/old-wiki/pages/Cookbook/Matplotlib/Show_colormaps
    ... and note that any colorbar there can have a "_r" as a suffix,
    to reverse it.

    """

    def __init__( self, m, fout='' ):

        self.m         = None
        
        self.file_out  = 'IMAGE'
        self.fout_base = ''
        self.fout_ext  = ''

        self.figsize   = (3.5, 2.8)    # (w, h), in inches
        self.dpi       = 100
        self.facecolor = 'w'
        self.interp    = 'nearest'
        self.aspect    = 'equal'
        self.cbar      = 'jet'
        self.vmin      = None
        self.vmax      = None

        self.plt_xticks_ON     = True
        self.plt_xticks_ticks  = None
        self.plt_xticks_labels = None
        self.plt_xticks_N      = 0
        self.plt_xticks_rot    = 45
        self.plt_xticks_ha     = 'right'
        self.plt_xticks_FS     = 10

        self.plt_yticks_ON     = True
        self.plt_yticks_ticks  = None
        self.plt_yticks_labels = None
        self.plt_yticks_N      = 0
        self.plt_yticks_rot    = 0
        self.plt_yticks_FS     = 10

        self.plt_title_txt     = ''
        self.plt_title_FS      = 10

        self.cbar_width_perc   = 5
        self.cbar_pad          = 0.1
        self.cbar_loc          = "right"
        self.cbar_n_interval   = 4
        self.cbar_num_form     = None   # try to guess int/float from vals


        self.do_tight_layout   = True
        self.do_hold_image     = False
        self.do_show_cbar      = True

        # -------------
        
        self.read_mat2d( m )
        self.set_plopts_from_mat2d()

        # set output filename (find or create ext)
        if fout :
            self.set_file_out( fout )
        else:
            self.set_file_out( self.file_out + '_' + self.m.label )

    # -------------------------------------------------------------

    def set_file_out(self, FF):
        """Set output file name, and parse for ext (or decide on one, if none
        is given.

        """      

        if not(FF) :
            ab.EP("No output file given for me to work with?")

        ff_split = FF.split('.')
        ext      = ff_split[-1]
        
        if len(ff_split) < 2 or \
           not(['bmp', 'emf', 'eps', 'gif', 'jpeg', 'jpg', \
                'pdf', 'pgf', 'png', 'ps', 'raw', 'rgba', \
                'svg', 'svgz', 'tif', 'tiff'].__contains__(ext)) : 
            # fname is just base; need to add our own ext
            self.ext       = rcParams["savefig.format"]
            self.fout_base = FF 
            self.file_out  = '.'.join([self.fout_base, self.ext])
        else:
            # fname is complete
            self.ext       = ext
            self.fout_base = ''.join(ff_split[:-1])
            self.file_out  = '.'.join([self.fout_base, self.ext])
    
    def read_mat2d( self, MM ):
        """Just read in the mat2d obj
        """
    
        self.m = MM

    def set_plopts_from_mat2d(self):
        
        # range of plot
        self.vmin = self.m.min_ele
        self.vmax = self.m.max_ele

        # yticks/labels
        if self.m.col_strlabs :
            self.plt_yticks_labels = self.m.col_strlabs
        elif self.m.col_intvals :
            self.plt_yticks_labels = [str(w) for w in self.m.col_intvals]
        else: 
            self.plt_yticks_labels = ['']*self.m.ncol # shouldn't happen
        self.plt_yticks_N      = len(self.plt_yticks_labels)
        self.plt_yticks_ticks  = [i for i in range(self.plt_yticks_N) ]

        # xticks/labels
        if self.m.row_strlabs :
            self.plt_xticks_labels = self.m.row_strlabs
        elif self.m.row_intvals :
            self.plt_xticks_labels = [str(w) for w in self.m.row_intvals]
        else: 
            self.plt_xticks_labels = ['']*self.m.nrow # shouldn't happen
        self.plt_xticks_N      = len(self.plt_xticks_labels)
        self.plt_xticks_ticks  = [i for i in range(self.plt_xticks_N) ]
        
        # plot title
        self.plt_title_txt = self.m.label

        

    def make_plot(self):
        """
        Construct the plot obj
        """

        fig = plt.figure( figsize=self.figsize, 
                          dpi=self.dpi, 
                          facecolor=self.facecolor)
        subb = plt.subplot(111)

        ax   = plt.gca()
        box  = ax.get_position()

        IM = ax.imshow( self.m.mat,
                        interpolation = self.interp,
                        aspect        = self.aspect,
                        vmin          = self.vmin,
                        vmax          = self.vmax,
                        cmap          = self.cbar )

        if self.plt_xticks_ON:
            plt.xticks( self.plt_xticks_ticks,
                        self.plt_xticks_labels,
                        rotation = self.plt_xticks_rot,
                        ha       = self.plt_xticks_ha,
                        fontsize = self.plt_xticks_FS )

        plt.yticks( self.plt_yticks_ticks,
                    self.plt_yticks_labels,
                    rotation = self.plt_yticks_rot,
                    fontsize = self.plt_yticks_FS )

        # this is do deal with an apparent bug in some versions of
        # matplotlib:
        # https://github.com/matplotlib/matplotlib/issues/14751
        ax.set_ylim(self.plt_yticks_N-0.5, -0.5)

        plt.title( self.plt_title_txt,
                   fontsize=self.plt_title_FS )

        if self.do_show_cbar :
            cbar_wid = "{}%".format(self.cbar_width_perc)
            divider  = make_axes_locatable(ax)
            TheCax   = divider.append_axes(self.cbar_loc, 
                                           size = cbar_wid, 
                                           pad  = self.cbar_pad )
            ntick    = self.cbar_n_interval + 1
            del_cbar = (self.vmax - self.vmin) / self.cbar_n_interval 
            ColBar   = [(self.vmin+del_cbar*ii) for ii in range(ntick) ]

            if self.cbar_num_form :
                cbar_num_form = self.cbar_num_form
            elif ab.list_count_float_not_int(ColBar) :
                # looks like floats
                max_cbar = max(ColBar)
                if max_cbar < 0.01 :
                    cbar_num_form = '%.2e'
                elif max_cbar < 10 :
                    cbar_num_form = '%.3f'
                elif max_cbar < 1000 :
                    cbar_num_form = '%.1f'
                else: 
                    cbar_num_form = '%.2e'
            else:
                # looks like ints
                max_cbar = max(ColBar)
                if max_cbar < 1000 :
                    cbar_num_form = '%d'
                else: 
                    cbar_num_form = '%.2e'

            cbar     = plt.colorbar( IM, 
                                     cax    = TheCax, 
                                     ticks  = ColBar, 
                                     format = cbar_num_form )

        if self.do_tight_layout :
            plt.tight_layout()
        else:
            subb.set_position([ box.x0+box.width*0.0, 
                                box.y0+box.height*0.05,  
                                box.width*0.95, 
                                box.height*0.95 ])

        # ------------ save -----------
        plt.savefig( self.file_out,
                     dpi = self.dpi )
        ab.IP("Made plot: {}".format(self.file_out))

        # ------------ display -------------
        if self.do_hold_image :
            plt.ion()
            plt.show()

            try:
                input()
            except:
                raw_input()




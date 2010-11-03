#IOApps, IO profiler and IO traces replayer
#
#    Copyright (C) 2010 Jiri Horky <jiri.horky@gmail.com>
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

import matplotlib.pyplot as plt
from matplotlib.collections import LineCollection
from matplotlib.ticker import FuncFormatter
from matplotlib.backends.backend_qt4agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.backends.backend_qt4agg import NavigationToolbar2QTAgg as NavigationToolbar
from matplotlib.figure import Figure
import numpy as np
import math
import os
import locale

def percentile(N, percent, key=lambda x:x):
    """
    Find the percentile of a list of values.

    @parameter N - is a list of values. Note N MUST BE already sorted.
    @parameter percent - a float value from 0.0 to 1.0.
    @parameter key - optional key function to compute value from each element of N.

    @return - the percentile of the values
    """
    if N == None:
        return None
    k = (len(N)-1) * percent
    f = math.floor(k)
    c = math.ceil(k)
    if f == c:
        return key(N[int(k)])
    d0 = key(N[int(f)]) * (k-f)
    d1 = key(N[int(c)]) * (c-k)
    return d0+d1


class DataHolder:
    def __init__(self):
        self.dict = None
        
    def setData(self, dict):
        self.dict = dict
                  
    def data(self):
        return self.dict
        
    def getSummary(self, type):
        if not self.dict:
            raise ValueError('Data not set')
        
        retDict = {}
        
        for name in self.dict[type].keys():
            ops = self.dict[type][name][1]
            data = np.array(ops, dtype=[('off', np.float64), ('size', np.float64),       
                                         ('start', np.float64), ('dur', np.float64)])         
            retDict[name] = [len(ops), max(data['size'] + data['off']), sum(data['size']), sum(data['dur'])]
        
        return retDict


class SubGrapher:    
    """Can graph...."""
    WRITES = 'writes'
    READS = 'reads'
    MAX_NAME = 80 #should be always divisible by 2
    timeModes = ['absolute', 'relapp', 'relfile', 'count']
    plotModes = ['show', 'save']

    def __init__(self):
        self.fileName = None
        self.data = None
        self.type = self.READS
        self.timeMode = self.timeModes[0]
        self.plotMode = self.plotModes[0]
        self.firtAppTime = 0
        self.lineCol = None
        self.dpi = 72
        self.axes = None
        self.fig = None
        self.dataHolder = None

    def __prepareData(self):
        if self.timeMode == "relapp":
            self.firstAppTime = self.dataHolder.data()[self.type][self.fileName][0][2]
        firstFileTime = self.dataHolder.data()[self.type][self.fileName][0] * 1000            
        #reads
        ops = self.dataHolder.data()[self.type][self.fileName][1]
        self.data = np.array(ops, dtype=[('off', np.float64), ('size', np.float64),
                  ('start', np.float64), ('dur', np.float64)])
        self.data['start'] = self.data['start'] * 1000;
            
        if self.timeMode == "relfile":
            self.startTime = 0
            self.data['start'] -= firstFileTime;
        elif self.timeMode == "relapp":
            self.startTime = 0    
            self.data['start'] -= self.firstAppTime;
        elif self.timeMode == "absolute":
            self.startTime = self.data['start'][0]
        else:
            self.startTime = 0        
    
    def __xFormater(self, x, pos):
        
        """Formatter for X axis"""
        return locale.format("%0.1f", x, grouping=True)
    
    def __elideText(self, text):
        if len(text) > self.MAX_NAME:
            return text[0:(self.MAX_NAME/2-2)] + "...." + text[-(self.MAX_NAME/2-2):]
        else:
            return text
    
    def __getTicks(self, low, high):
        minTicks = 10
        maxTicks = 20

        ticks = []        
        div = int(1)
        pow = 0

        while  high / div > maxTicks:
            pow+=1
            div*=10

        tick = 0;
        while tick < high:
            ticks.append(tick)
            tick += div

        if pow == 0:
                last = round(high)
        else:
            last = round(high + div/2.0, 0 - pow)

        ticks.append(last)

        return ticks

    def setTimeMode(self, mode):
        if mode in self.timeModes:
            self.timeMode = mode;
        else:
            print 'Time mode not in allowed list'
    
    def timeMode(self):
        return self.timeMode
    
    def setFig(self, fig):
        self.fig = fig
        
    def setAxes(self, axes):
        self.axes = axes
    
    def setDataHolder(self, dataHolder):
        self.dataHolder = dataHolder
    
    def dataHolder(self):
        return self.dataHolder

    def setPlotMode(self, mode):
        if mode in self.plotModes:
            self.plotMode = mode;
        else:
            print 'Plot mode not in allowed list'
    
    def setName(self, name):
        self.fileName = name
        
    def name(self):
        return self.fileName
    
    def setType(self, type):
        self.type = type        
    
    def plotMode(self):
        return self.plotMode    

    def filenames(self, type):
        return self.dataHolder.data()[type].keys()
    
    def lineCollection(self):
        return self.lineCol  

    def plotHist(self, data, xlabel, ylabel):            
        self.axes.clear()
        
        mean = np.mean(data, dtype=np.float64);
        std = np.std(data, dtype=np.float64);            
        rangeMin = mean-2*std
        rangeMax = mean+2*std
        if rangeMin < 0:
            rangeMin = 0
        self.axes.hist(data, 50, facecolor='green', range=(rangeMin, rangeMax))
        
        self.axes.set_xlabel(xlabel)
        self.axes.set_ylabel(ylabel)
                    
    def plotHistSize(self):
        if self.data == None:
            self.__prepareData
            
        self.axes.clear()                    
        self.axes.hist(self.data['size'], 50, facecolor='green', range=(rangeMin, rangeMax))
        self.axes.set_xlabel(xlabel);
        self.axes.set_ylabel(ylabel);
            
    def plotHistSize(self):
        if self.data == None:
            self.__prepareData
            
        self.plotHist(self.data['size'], "request size (kiB)", "count")        
        
    def plotHistTime(self):
        if self.data == None:
            self.__prepareData
            
        self.plotHist(self.data['dur'], "request duration (ms)", "count")                                                

    def plotHistSpeed(self):
        if self.data == None:
            self.__prepareData
            
        self.plotHist(self.data['size']/self.data['dur']*1000, "request speed (kiB/s)", "count")
                
    def plotPattern(self, type, stats):
        """Common graph function for read and writes, type is either 'read' or 'write'. Stats is a bool that indicates whether to print statistics or not."""
        names = self.filenames(type) #unique names of used files                
        if not self.fileName in names:
            print self.fileName, "is not in our data set"             
            return                    
                                    
        if self.data == None:
            self.__prepareData()
            
        self.axes.clear()
        
        graphdata = np.column_stack((self.data['off'], self.data['start'], self.data['off']+ self.data['size'], self.data['start'] + self.data['dur']))

        lineSegments = LineCollection(graphdata.reshape(-1,2,2), linewidths=(8));
        lineSegments.set_picker(True)        
        self.lineCol = self.axes.add_collection(lineSegments)
    
        maxEnd = max(graphdata[:,2])
        maxTime = max(graphdata[:,3])
                                                    
        if stats:
            sizeMean = np.mean(self.data['size'], dtype=np.float64);
            sizeStd = np.std(self.data['size'], dtype=np.float64);
            durMean = np.mean(self.data['dur'], dtype=np.float64);
            durStd = np.std(self.data['dur'], dtype=np.float64);
    
                                                    
            sumSize = sum(self.data['size'])
            sumDur = sum(self.data['dur'])		  
            text = type +  " total %0.1lf kB in %ld calls (avg. %0.1f kiB/s)\n Size avg[kiB] %0.1f +- %0.1f, max: %0.1lf, min: %0.1lf\n Dur[ms]: Total: %0.3lf, Avg: %0.3f +- %0.3lf, max: %0.3lf, min: %0.3lf"\
                % (sumSize, len(self.data['size']), (sumSize/sumDur)*1000, sizeMean, sizeStd, max(self.data['size']), min(self.data['size']), sumDur, durMean, durStd, max(self.data['dur']), min(self.data['size']))
            self.axes.text(0.95, 0.05, text, horizontalalignment='right', verticalalignment='bottom', transform=self.axes.transAxes, fontsize=12)

        self.axes.xaxis.set_major_formatter(FuncFormatter(self.__xFormater))
        self.axes.grid(color='grey', linewidth=0.5)
        self.axes.set_xlabel("file offset (kiB)", fontsize=16);
        self.axes.set_ylabel("time (ms)", fontsize=16);
        self.axes.set_xlim(0, maxEnd);                        
        self.axes.set_ylim(self.startTime, maxTime);
        
        self.fig.suptitle('%s' % self.__elideText(self.fileName), fontsize=9)
#            ticks = self.__getTicks(0, maxEnd)
#            plt.xticks(ticks);
        self.fig.autofmt_xdate()            
            
    def plotReads(self, stats):
            self.plotPattern(self.READS, stats)
            
    def plotWrites(self, stats):
            self.plotPattern(self.WRITES, stats)
    

class Grapher(SubGrapher):
    """Can graph...."""

    def __init__(self):
        SubGrapher.__init__()
        fig = plt.figure()
        ax = plt.axes()
        self.setFig(fig)
        self.setAxes(ax)
    
    def plot(self, type):
        SubGrapher.plot(self, type)
        plt.show();        

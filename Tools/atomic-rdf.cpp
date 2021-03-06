/*
  Compute 3d radial distribution function for two selections.
  This version treats each atom in each selection independently.  If you
  want to look at the distribution of centers of mass, look at rdf instead.

  Alan Grossfield
  Grossfield Lab
  Department of Biochemistry and Biophysics
  University of Rochester Medical School
 
  This file is part of LOOS.

  LOOS (Lightweight Object-Oriented Structure library)
  Copyright (c) 2008, Alan Grossfield
  Department of Biochemistry and Biophysics
  School of Medicine & Dentistry, University of Rochester

  This package (LOOS) is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation under version 3 of the License.

  This package is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <loos.hpp>

using namespace std;
using namespace loos;

namespace opts = loos::OptionsFramework;
namespace po = loos::OptionsFramework::po;



void Usage()
    {
    cerr << "Usage: atomic-rdf system trajectory selection1 selection2 "
         << "histogram-min histogram-max histogram-bins skip" 
         << endl;
    }

string fullHelpMessage(void)
    {
    string s = 
    "\n"
    "SYNOPSIS\n"
    "\n"
    "\tCompute the radial distribution function for two selections of atoms\n"
    "\n"
    "DESCRIPTION\n"
    "\n"
    "\tThis tool computes the radial distribution function for two selections\n"
    "of atoms, treating them as individual atoms rather than groups.  This is\n"
    "in contrast to the tool rdf, which treats them as groups.\n"
    "\n"
    "The output columns have the following meaning:\n"
    "    1: distance\n"
    "    2: normalized RDF\n"
    "    3: cumulative distribution function of selection-2 atoms around \n"
    "       selection-1 atoms\n"
    "    4: cumulative distribution function of selection-1 atoms around \n"
    "       selection-2 atoms\n"
    "\n"
    "EXAMPLE\n"
    "\n"
    "  atomic-rdf model traj 'name =~ \"OP[1-4]\"' 'name =~ \"OH2\" && \\\n"
    "             resname == \"TIP3\"' 0 20 40\n"
    "will compute the radial distribution function for phosphate oxygens and\n"
    "water oxygens, treating each phosphate oxygen independently.  Using the \n"
    "same selections with the rdf tool would likely group the 4 phosphate \n"
    "oxygens from each lipid into one unit and use their center of mass.\n"
    "\n"
    "As with the other rdf tools (rdf, xy_rdf), histogram-min, histogram-max,\n"
    "and histogram-bins control the range over which the rdf is computed, and\n"
    "the number of bins used, in this case from 0 to 20 Angstroms, with 0.5\n"
    "angstrom bins.\n";
    return(s);
    }

int main (int argc, char *argv[])
{

// Build options
opts::BasicOptions* bopts = new opts::BasicOptions(fullHelpMessage());
opts::TrajectoryWithFrameIndices* tropts = new opts::TrajectoryWithFrameIndices;
opts::RequiredArguments* ropts = new opts::RequiredArguments;

// These are required command-line arguments (non-optional options)
ropts->addArgument("selection1", "selection1");
ropts->addArgument("selection2", "selection2");
ropts->addArgument("min", "min radius");
ropts->addArgument("max", "max radius");
ropts->addArgument("num_bins", "number of bins");

opts::AggregateOptions options;
options.add(bopts).add(tropts).add(ropts);
if (!options.parse(argc, argv))
  exit(-1);


// Print the command line arguments
cout << "# " << invocationHeader(argc, argv) << endl;

// copy the command line variables to real variable names
// Create the system and read the trajectory file
AtomicGroup system = tropts->model;
pTraj traj = tropts->trajectory;
if (!(system.isPeriodic() || traj->hasPeriodicBox()))
  {
  cerr << "Error- Either the model or the trajectory must have periodic box information.\n";
  exit(-1);
  }

// Extract our required command-line arguments
string selection1 = ropts->value("selection1");  // String describing the first selection
string selection2 = ropts->value("selection2");  // String describing the second selection
double hist_min = parseStringAs<double>(ropts->value("min")); // Lower edge of the histogram
double hist_max = parseStringAs<double>(ropts->value("max")); // Upper edge of the histogram
int num_bins = parseStringAs<double>(ropts->value("num_bins")); // Number of bins in the histogram


double bin_width = (hist_max - hist_min)/num_bins;


// Set up the selector to define group1 atoms
AtomicGroup group1 = selectAtoms(system, selection1);
if (group1.empty())
    {
    cerr << "Error- no atoms selected by '" << selection1 << "'\n";
    exit(-1);
    }

// Set up the selector to define group2 atoms
AtomicGroup group2 = selectAtoms(system, selection2);
if (group2.empty())
    {
    cerr << "Error- no atoms selected by '" << selection2 << "'\n";
    exit(-1);
    }

// Create the histogram and zero it out
vector<double> hist;
hist.reserve(num_bins);
hist.insert(hist.begin(), num_bins, 0.0);

double min2 = hist_min*hist_min;
double max2 = hist_max*hist_max;

// loop over the frames of the trajectory
vector<uint> framelist = tropts->frameList();
uint framecnt = framelist.size();
double volume = 0.0;
unsigned long unique_pairs=0;
for (uint index = 0; index<framecnt; ++index)
    {
    traj->readFrame(framelist[index]);

    // update coordinates and periodic box
    traj->updateGroupCoords(system);
    GCoord box = system.periodicBox(); 
    volume += box.x() * box.y() * box.z();

    unique_pairs = 0;
    // compute the distribution of g2 around g1 
    for (uint j = 0; j < group1.size(); j++)
        {
        pAtom a1 = group1[j];
        GCoord p1 = a1->coords();
        for (uint k = 0; k < group2.size(); k++)
            {
            pAtom a2 = group2[k];
            // skip "self" pairs 
            if (a1 == a2)
                {
                continue;
                }
            unique_pairs++;
            GCoord p2 = a2->coords();
            // Compute the distance squared, taking periodicity into account
            double d2 = p1.distance2(p2, box);
            if ( (d2 < max2) && (d2 > min2) )
                {
                double d = sqrt(d2);
                int bin = int((d-hist_min)/bin_width);
                hist[bin]++;
                }
            }
        }
    }

volume /= framecnt;



double expected = framecnt * unique_pairs / volume;
double cum1 = 0.0;
double cum2 = 0.0;

// Output the results
cout << "# Dist\tRDF\tCumAround1\tCumAround2" << endl;
for (int i = 0; i < num_bins; i++)
    {
    double d = bin_width*(i + 0.5);

    double d_inner = bin_width*i;
    double d_outer = d_inner + bin_width;
    double norm = 4.0/3.0 * M_PI*(d_outer*d_outer*d_outer 
                                - d_inner*d_inner*d_inner);

    double total = hist[i]/ (norm*expected);
    cum1 += hist[i] / (framecnt*group1.size());
    cum2 += hist[i] / (framecnt*group2.size());

    cout << d << "\t" << total << "\t" 
         << cum1 << "\t" << cum2 << endl;

    }
}


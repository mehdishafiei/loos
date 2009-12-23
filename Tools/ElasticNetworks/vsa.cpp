/*
  vsa

  (c) 2009 Tod D. Romo, Grossfield Lab
      Department of Biochemistry
      University of Rochster School of Medicine and Dentistry


  Usage:
    vsa subset environment radius model output_prefix

*/


/*
  This file is part of LOOS.

  LOOS (Lightweight Object-Oriented Structure library)
  Copyright (c) 2009 Tod D. Romo
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

#include <boost/format.hpp>
#include <boost/program_options.hpp>


using namespace std;
using namespace loos;
namespace po = boost::program_options;


typedef pair<uint,uint> Range;



// Globals...
double normalization = 1.0;
double threshold = 1e-10;

string subset_selection, environment_selection, model_name, prefix, mass_file;
double cutoff;
int verbosity = 0;
bool occupancies_are_masses;



void parseOptions(int argc, char *argv[]) {

  try {
    po::options_description generic("Allowed options");
    generic.add_options()
      ("help", "Produce this help message")
      ("cutoff,c", po::value<double>(&cutoff)->default_value(15.0), "Cutoff distance for node contact")
      ("masses,m", po::value<string>(&mass_file), "Name of file that contains atom mass assignments")
      ("verbosity,v", po::value<int>(&verbosity)->default_value(0), "Verbosity level")
      ("occupancies,o", po::value<bool>(&occupancies_are_masses)->default_value(false), "Atom masses are stored in the PDB occupancy field");


    po::options_description hidden("Hidden options");
    hidden.add_options()
      ("subset", po::value<string>(&subset_selection), "Subset selection")
      ("env", po::value<string>(&environment_selection), "Environment selection")
      ("model", po::value<string>(&model_name), "Model filename")
      ("prefix", po::value<string>(&prefix), "Output prefix");
    
    po::options_description command_line;
    command_line.add(generic).add(hidden);

    po::positional_options_description p;
    p.add("subset", 1);
    p.add("env", 1);
    p.add("model", 1);
    p.add("prefix", 1);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).
              options(command_line).positional(p).run(), vm);
    po::notify(vm);

    if (vm.count("help") || !(vm.count("model") && vm.count("prefix") && vm.count("subset") && vm.count("env"))) {
      cerr << "Usage- vsa [options] subset environment model-name output-prefix\n";
      cerr << generic;
      exit(-1);
    }
  }
  catch(exception& e) {
    cerr << "Error - " << e.what() << endl;
    exit(-1);
  }
}






DoubleMatrix hblock(const int i, const int j, const AtomicGroup& model, const double radius2) {

  DoubleMatrix B(3,3);
  GCoord u = model[i]->coords();
  GCoord v = model[j]->coords();
  GCoord d = v - u;

  double s = d.length2();
  if (s <= radius2) {

    for (int j=0; j<3; ++j)
      for (int i=0; i<3; ++i)
        B(i,j) = normalization * d[i]*d[j] / s;
  }

  return(B);
}



DoubleMatrix hessian(const AtomicGroup& model, const double radius) {
  
  int n = model.size();
  DoubleMatrix H(3*n,3*n);
  double r2 = radius * radius;

  for (int i=1; i<n; ++i) {
    for (int j=0; j<i; ++j) {
      DoubleMatrix B = hblock(i, j, model, r2);
      for (int x = 0; x<3; ++x)
        for (int y = 0; y<3; ++y) {
          H(i*3 + y, j*3 + x) = -B(y, x);
          H(j*3 + x, i*3 + y) = -B(x ,y);
        }
    }
  }

  // Now handle the diagonal...
  for (int i=0; i<n; ++i) {
    DoubleMatrix B(3,3);
    for (int j=0; j<n; ++j) {
      if (j == i)
        continue;
      
      for (int x=0; x<3; ++x)
        for (int y=0; y<3; ++y)
          B(y,x) += H(j*3 + y, i*3 + x);
    }

    for (int x=0; x<3; ++x)
      for (int y=0; y<3; ++y)
        H(i*3 + y, i*3 + x) = -B(y,x);
  }

  return(H);
}



DoubleMatrix submatrix(const DoubleMatrix& M, const Range& rows, const Range& cols) {
  uint m = rows.second - rows.first;
  uint n = cols.second - cols.first;

  DoubleMatrix A(m,n);
  for (uint i=0; i < n; ++i)
    for (uint j=0; j < m; ++j)
      A(j,i) = M(j+rows.first, i+cols.first);

  return(A);
}



boost::tuple<DoubleMatrix, DoubleMatrix> eigenDecomp(DoubleMatrix& A, DoubleMatrix& B) {

  DoubleMatrix AA = A.copy();
  DoubleMatrix BB = B.copy();

  DoubleMatrix Bi = invert(BB);
  AA *= Bi;
  boost::tuple<DoubleMatrix, DoubleMatrix, DoubleMatrix> res = svd(AA);
  DoubleMatrix U = boost::get<0>(res);
  DoubleMatrix S = boost::get<1>(res);

  vector<uint> indices = sortedIndex(S);
  S = permuteRows(S, indices);
  U = permuteColumns(U, indices);

  boost::tuple<DoubleMatrix, DoubleMatrix> result(S, U);
  return(result);
}


void assignMasses(AtomicGroup& grp, const string& name) {
  ifstream ifs(name.c_str());
  if (!ifs) {
    cerr << "Warning- no masses will be used\n";
    return;
  }

  double mass;
  string pattern;
  bool no_masses_set = true;

  while (ifs >> pattern >> mass) {
    string selection("name =~ '");
    selection += pattern + "'";
    Parser parser(selection);
    KernelSelector selector(parser.kernel());
    AtomicGroup subset = grp.select(selector);
    if (subset.empty())
      continue;
    no_masses_set = false;
    if (verbosity > 1)
      cout << "Assigning " << subset.size() << " atoms with pattern '" << pattern << "' to mass " << mass << endl;
    for (AtomicGroup::iterator i = subset.begin(); i != subset.end(); ++i)
      (*i)->mass(mass);
  }

  if (no_masses_set)
    cerr << "WARNING- no masses were assigned\n";
}


DoubleMatrix getMasses(const AtomicGroup& grp) {
  uint n = grp.size();

  DoubleMatrix M(3*n,3*n);
  for (uint i=0, k=0; i<n; ++i, k += 3) {
    M(k,k) = grp[i]->mass();
    M(k+1,k+1) = grp[i]->mass();
    M(k+2,k+2) = grp[i]->mass();
  }

  return(M);
}


void showSize(const string& s, const DoubleMatrix& M) {
  cerr << s << M.rows() << " x " << M.cols() << endl;
}


int main(int argc, char *argv[]) {
  string hdr = invocationHeader(argc, argv);
  parseOptions(argc, argv);

  AtomicGroup model = createSystem(model_name);
  if (occupancies_are_masses) {

    cerr << "Assigning masses from occupancies...\n";
    for (AtomicGroup::iterator i = model.begin(); i != model.end(); ++i)
      (*i)->mass((*i)->occupancy());

  } else {
    if (mass_file.empty())
      cerr << "WARNING- using default masses\n";
    else
      assignMasses(model, mass_file);
  }

  AtomicGroup subset = selectAtoms(model, subset_selection);
  AtomicGroup environment = selectAtoms(model, environment_selection);

  AtomicGroup composite = subset + environment;

  if (verbosity > 1) {
    cerr << "Subset size is " << subset.size() << endl;
    cerr << "Environment size is " << environment.size() << endl;
  }

  DoubleMatrix H = hessian(composite, cutoff);

  // Now, burst out the subparts...
  uint l = subset.size() * 3;

  uint n = H.cols();

  ScientificMatrixFormatter<double> sp(24,18);

  DoubleMatrix Hss = submatrix(H, Range(0,l), Range(0,l));
  if (verbosity > 1)
    showSize("Hss = ", Hss);
  writeAsciiMatrix("Hss.asc", Hss, "", false, sp);

  DoubleMatrix Hee = submatrix(H, Range(l, n), Range(l, n));
  if (verbosity > 1)
    showSize("Hee = ", Hee);
  writeAsciiMatrix("Hee.asc", Hee, "", false, sp);

  DoubleMatrix Hse = submatrix(H, Range(0,l), Range(l, n));
  writeAsciiMatrix("Hse.asc", Hse, "", false, sp);
  DoubleMatrix Hes = submatrix(H, Range(l, n), Range(0, l));
  writeAsciiMatrix("Hes.asc", Hes, "", false, sp);


  Timer<WallTimer> timer;
  if (verbosity > 0) {
    cerr << "Inverting environment hessian...\n";
    timer.start();
    if (verbosity > 1)
      showSize("Hee = ", Hee);
  }

  DoubleMatrix Heei = Math::invert(Hee);
  if (verbosity > 0) {
    timer.stop();
    cerr << timer << endl;
  }
  writeAsciiMatrix("Heei.asc", Heei, "", false, sp);

  DoubleMatrix Hssp = Hss - Hse * Heei * Hes;
  writeAsciiMatrix("Hssp.asc", Hssp, "", false, sp);
  DoubleMatrix Ms = getMasses(subset);
  writeAsciiMatrix("Ms.asc", Ms, "", false, sp);
  DoubleMatrix Me = getMasses(environment);
  if (verbosity > 1)
    showSize("Me = ", Me);

  writeAsciiMatrix("Me.asc", Me, "", false, sp);
  DoubleMatrix Msp = Ms + Hse * Heei * Me * Heei * Hes;
  writeAsciiMatrix("Msp.asc", Hss, "", false, sp);

  //writeAsciiMatrix(prefix + "_Hssp.asc", Hssp, hdr);
  //writeAsciiMatrix(prefix + "_Msp.asc", Msp, hdr);

  if (verbosity > 0) {
    cerr << "Running eigendecomp of " << Hssp.rows() << " x " << Hssp.cols() << " matrix ...";
    timer.start();
  }
  boost::tuple<DoubleMatrix, DoubleMatrix> eigenpairs = eigenDecomp(Hssp, Msp);
  if (verbosity > 0) {
    timer.stop();
    cerr << "done\n";
    cerr << timer << endl;
  }
    
  DoubleMatrix Ds = boost::get<0>(eigenpairs);
  DoubleMatrix Us = boost::get<1>(eigenpairs);

  writeAsciiMatrix(prefix + "_Ds.asc", Ds, hdr);
  writeAsciiMatrix(prefix + "_Us.asc", Us, hdr);
}

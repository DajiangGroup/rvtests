#include "ModelFitter.h"

#include "Model.h"
#include "ModelParser.h"

#include "regression/GSLIntegration.h"

//////////////////////////////////////////////////////////////////////
// Implementation of various collpasing methods
#if 0
/**
 * @return allele frequency
 * NOTE: Madson-Browning definition of alleleFrequency is different
 *       double freq = 1.0 * (ac + 1) / (an + 1);
 */
double getMarkerFrequency(Matrix& in, int col) {
  int& numPeople = in.rows;
  double ac = 0;  // NOTE: here genotype may be imputed, thus not integer
  int an = 0;
  for (int p = 0; p < numPeople; p++) {
    if ((int)in[p][col] >= 0) {
      ac += in[p][col];
      an += 2;
    }
  }
  if (an == 0) return 0.0;
  double freq = ac / an;
  return freq;
}

void getMarkerFrequency(Matrix& in, std::vector<double>* freq) {
  freq->resize(in.cols);
  for (int i = 0; i < in.cols; ++i) {
    (*freq)[i] = getMarkerFrequency(in, i);
  }
}
#endif
double getMarkerFrequency(DataConsolidator* dc, int col) {
  return dc->getMarkerFrequency(col);
}

void getMarkerFrequency(DataConsolidator* dc, std::vector<double>* freq) {
  return dc->getMarkerFrequency(freq);
}

double getMarkerFrequencyFromControl(Matrix& in, Vector& pheno, int col) {
  int& numPeople = in.rows;
  double ac = 0;  // NOTE: here genotype may be imputed, thus not integer
  int an = 0;
  for (int p = 0; p < numPeople; p++) {
    if (pheno[p] == 1) continue;
    if (in[p][col] >= 0) {
      ac += in[p][col];
      an += 2;
    }
  }
  // Refer:
  // 1. Madsen BE, Browning SR. A Groupwise Association Test for Rare Mutations
  // Using a Weighted Sum Statistic. PLoS Genet. 2009;5(2):e1000384. Available
  // at: http://dx.doi.org/10.1371/journal.pgen.1000384 [Accessed November 24,
  // 2010].
  double freq = 1.0 * (ac + 1) / (an + 2);
  return freq;
}

/**
 * Collapsing and combine method (indicator of existence of alternative allele)
 * @param in : sample by marker matrix
 * @param out: sample by 1 matrix
 */
void cmcCollapse(DataConsolidator* dc, Matrix& in, Matrix* out) {
  assert(out);
  int numPeople = in.rows;
  int numMarker = in.cols;

  out->Dimension(numPeople, 1);
  out->Zero();
  for (int p = 0; p < numPeople; p++) {
    for (int m = 0; m < numMarker; m++) {
      int g = (int)(in[p][m]);
      if (g > 0) {
        (*out)[p][0] = 1.0;
        break;
      }
    }
  }
}

void cmcCollapse(DataConsolidator* dc, Matrix& in,
                 const std::vector<int>& index, Matrix* out, int outIndex) {
  assert(out);
  int numPeople = in.rows;
  assert(out->rows == numPeople);
  assert(out->cols > outIndex);

  for (int p = 0; p < numPeople; p++) {
    (*out)[p][outIndex] = 0.0;
    for (size_t m = 0; m < index.size(); m++) {
      int g = (int)(in[p][index[m]]);
      if (g > 0) {
        (*out)[p][outIndex] = 1.0;
        break;
      }
    };
  };
}

/**
 * Morris-Zeggini method (count rare variants).
 * @param in : sample by marker matrix
 * @param out: sample by 1 matrix
 */
void zegginiCollapse(DataConsolidator* dc, Matrix& in, Matrix* out) {
  assert(out);
  int numPeople = in.rows;
  int numMarker = in.cols;

  out->Dimension(numPeople, 1);
  out->Zero();
  for (int p = 0; p < numPeople; p++) {
    for (int m = 0; m < numMarker; m++) {
      int g = (int)(in[p][m]);
      if (g > 0) {  // genotype is non-reference
        (*out)[p][0] += 1.0;
      }
    }
  }
}

void zegginiCollapse(DataConsolidator* dc, Matrix& in,
                     const std::vector<int>& index, Matrix* out, int outIndex) {
  assert(out);
  int numPeople = in.rows;
  assert(out->rows == numPeople);
  assert(out->cols > outIndex);

  for (int p = 0; p < numPeople; p++) {
    (*out)[p][outIndex] = 0.0;
    for (size_t m = 0; m < index.size(); m++) {
      int g = (int)(in[p][index[m]]);
      if (g > 0) {
        (*out)[p][outIndex] += 1.0;
      }
    };
  };
}

/**
 * @param genotype : people by marker matrix
 * @param phenotype: binary trait (0 or 1)
 * @param out: collapsed genotype
 */
void madsonBrowningCollapse(DataConsolidator* dc, Matrix& genotype,
                            Vector& phenotype, Matrix* out) {
  assert(out);
  int& numPeople = genotype.rows;
  int numMarker = genotype.cols;

  out->Dimension(numPeople, 1);
  out->Zero();

  for (int m = 0; m < numMarker; m++) {
    // calculate weight
    double freq = getMarkerFrequencyFromControl(genotype, phenotype, m);
    if (freq <= 0.0 || freq >= 1.0) continue;  // avoid freq == 1.0
    double weight = 1.0 / sqrt(freq * (1.0 - freq) * genotype.rows);
    // fprintf(stderr, "freq = %f\n", freq);

    for (int p = 0; p < numPeople; p++) {
      (*out)[p][0] += genotype[p][m] * weight;
    }
  }
};

void fpCollapse(DataConsolidator* dc, Matrix& in, Matrix* out) {
  assert(out);
  int& numPeople = in.rows;
  int numMarker = in.cols;

  out->Dimension(numPeople, 1);
  out->Zero();

  for (int m = 0; m < numMarker; m++) {
    // calculate weight
    // double freq = getMarkerFrequency(in, m);
    double freq = dc->getMarkerFrequency(m);
    if (freq <= 0.0 || freq >= 1.0) continue;  // avoid freq == 1.0
    double weight = 1.0 / sqrt(freq * (1.0 - freq));
    // fprintf(stderr, "freq = %f\n", freq);

    for (int p = 0; p < numPeople; p++) {
      (*out)[p][0] += in[p][m] * weight;
    }
  }
}

void madsonBrowningCollapse(DataConsolidator* dc, Matrix* d, Matrix* out) {
  assert(out);
  Matrix& in = (*d);
  int& numPeople = in.rows;
  int numMarker = in.cols;

  out->Dimension(numPeople, 1);
  out->Zero();

  for (int m = 0; m < numMarker; m++) {
    // calculate weight
    // double freq = getMarkerFrequency(in, m);
    double freq = dc->getMarkerFrequency(m);
    if (freq <= 0.0 || freq >= 1.0) continue;  // avoid freq == 1.0
    double weight = 1.0 / sqrt(freq * (1.0 - freq));
    // fprintf(stderr, "freq = %f\n", freq);

    for (int p = 0; p < numPeople; p++) {
      (*out)[p][0] += in[p][m] * weight;
    }
  };
}

/**
 * Convert genotype back to reference allele count
 * e.g. genotype 2 means homAlt/homAlt, so it has reference allele count 0
 */
/**
 * Convert genotype back to reference allele count
 * e.g. genotype 2 means homAlt/homAlt, so it has reference allele count 0
 */
void convertToReferenceAlleleCount(Matrix* g) {
  Matrix& m = *g;
  for (int i = 0; i < m.rows; ++i) {
    for (int j = 0; j < m.cols; ++j) {
      m[i][j] = 2 - m[i][j];
    }
  }
}

void convertToReferenceAlleleCount(Matrix& in, Matrix* g) {
  Matrix& m = *g;
  m = in;
  convertToReferenceAlleleCount(&m);
}

/**
 * group genotype by its frequency
 * @param in: sample by marker genotype matrix
 * @param out: key: frequency value:0-based index for freq
 * e.g. freq = [0.1, 0.2, 0.1, 0.3]  =>
 *      *group = {0.1: [0, 2], 0.2: 1, 0.3 : 3}
 * NOTE: due to rounding errors, we only keep 6 digits
 */
void groupFrequency(const std::vector<double>& freq,
                    std::map<double, std::vector<int> >* group) {
  group->clear();
  for (size_t i = 0; i != freq.size(); ++i) {
    double f = ceil(1000000. * freq[i]) / 1000000;
    (*group)[f].push_back(i);
  }
}

#if 0

/**
 * Collapsing @param in (people by marker) to @param out (people by marker),
 * if @param freqIn is empty, then frequncy is calculated from @param in
 * or according to @param freqIn to rearrange columns of @param in.
 * Reordered frequency are stored in @param freqOut, in ascending order
 */
void rearrangeGenotypeByFrequency(Matrix& in, const std::vector<double>& freqIn,
                                  Matrix* out, std::vector<double>* freqOut) {
  std::map<double, std::vector<int> > freqGroup;
  std::map<double, std::vector<int> >::const_iterator freqGroupIter;
  if (freqIn.empty()) {
    getMarkerFrequency(in, freqOut);
    groupFrequency(*freqOut, &freqGroup);
  } else {
    groupFrequency(freqIn, &freqGroup);
  }

  Matrix& sortedGenotype = *out;
  sortedGenotype.Dimension(in.rows, freqGroup.size());
  sortedGenotype.Zero();
  freqOut->clear();
  int idx = 0;
  for (freqGroupIter = freqGroup.begin(); freqGroupIter != freqGroup.end();
       freqGroupIter++) {
    freqOut->push_back(freqGroupIter->first);
    const std::vector<int>& cols = freqGroupIter->second;
    for (size_t j = 0; j != cols.size(); ++j) {
      for (int i = 0; i < in.rows; ++i) {
        sortedGenotype[i][cols[j]] += in[i][cols[j]];
      }
    }
    ++idx;
  }
}
#endif

void makeVariableThreshodlGenotype(
    DataConsolidator* dc, Matrix& in, const std::vector<double>& freqIn,
    Matrix* out, std::vector<double>* freqOut,
    void (*collapseFunc)(DataConsolidator*, Matrix&, const std::vector<int>&,
                         Matrix*, int)) {
  assert((int)freqIn.size() == in.cols);
  assert(freqIn.size());
  assert(out);
  assert(freqOut);

  std::map<double, std::vector<int> > freqGroup;
  std::map<double, std::vector<int> >::const_iterator freqGroupIter;

  groupFrequency(freqIn, &freqGroup);

  Matrix& sortedGenotype = *out;
  sortedGenotype.Dimension(in.rows, freqGroup.size());
  sortedGenotype.Zero();
  freqOut->resize(freqGroup.size());
  int idx = 0;
  std::vector<int> cumCols;
  for (freqGroupIter = freqGroup.begin(); freqGroupIter != freqGroup.end();
       freqGroupIter++) {
    (*freqOut)[idx] = freqGroupIter->first;
    const std::vector<int>& cols = freqGroupIter->second;
    for (size_t i = 0; i != cols.size(); ++i) {
      cumCols.push_back(cols[i]);
    }
    (*collapseFunc)(dc, in, cumCols, out, idx);

#if 0
    printf("In:\n");
    print(in);
    printf("Out:\n");
    print(*out);
#endif
    ++idx;
  }
}

double fIntegrand(double x, void* param) {
  if (x > 500 || x < -500) return 0.0;
  const double alpha = *((double*)param);
  const double tmp = exp(alpha + x);
  const double k = 1.0 / sqrt(2.0 * 3.1415926535897);
  const double ret = tmp / (1. + tmp) / (1. + tmp) * k * exp(-x * x * 0.5);
  // fprintf(stderr, "alpha = %g\tx = %g\t ret = %g\n", alpha, x, ret);
  return ret;
}

int obtainB(double alpha, double* out) {
  Integration i;
  gsl_function F;
  F.function = &fIntegrand;
  F.params = &alpha;
  if (i.integrate(F)) {
    fprintf(stderr, "Calculation of b may be inaccurate.\n");
    return -1;
  }
  *out = i.getResult();
  return 0;
}

void makeVariableThreshodlGenotype(
    DataConsolidator* dc, Matrix& in, Matrix* out, std::vector<double>* freqOut,
    void (*collapseFunc)(DataConsolidator*, Matrix&, const std::vector<int>&,
                         Matrix*, int)) {
  std::vector<double> freqIn;
  // getMarkerFrequency(in, &freqIn);
  dc->getMarkerFrequency(&freqIn);

  makeVariableThreshodlGenotype(dc, in, freqIn, out, freqOut, collapseFunc);
}

void SingleVariantScoreTest::calculateConstant(Matrix& phenotype) {
  int nCase = 0;
  int nCtrl = 0;
  for (int i = 0; i < phenotype.rows; ++i) {
    if (phenotype[i][0] == 1) {
      ++nCase;
    } else if (phenotype[i][0] == 0) {
      ++nCtrl;
    }
  }
  double alpha;
  if (nCtrl > 0) {
    alpha = log(1.0 * nCase / nCtrl);
  } else {
    alpha = 500.;
  }
  obtainB(alpha, &this->b);
}
void MetaScoreTest::MetaFamBinary::calculateB() {
  obtainB(this->alpha, &this->b);
  // fprintf(stderr, "alpha = %g, b = %g\n", alpha, b);
  return;
}
void MetaCovTest::MetaCovFamBinary::calculateB() {
  obtainB(this->alpha, &this->b);
  return;
}

void MetaScoreTest::MetaUnrelatedBinary::calculateB() {
  obtainB(this->alpha, &this->b);
  return;
}

#if 0
void appendHeritability(FileWriter* fp, const FastLMM& model) {
  return;

  // TODO: handle empiricalkinship and pedigree kinship better
  // we estimate sigma2_g * K, but a formal defiinte is 2 * sigma2_g *K,
  // so multiply 0.5 to scale it.
  const double sigma2_g = model.GetSigmaG2() * 0.5;
  const double sigma2_e = model.GetSigmaE2();
  const double herit = (sigma2_g + sigma2_e == 0.) ? 0 : sigma2_g / (sigma2_g + sigma2_e);

  fp->printf("#Sigma2_g\t%g\n", sigma2_g);
  fp->printf("#Sigma2_e\t%g\n", sigma2_e);
  fp->printf("#Heritability\t%g\n", herit);
}

void appendHeritability(FileWriter* fp, const GrammarGamma& model) {
  return;

  // TODO: handle empiricalkinship and pedigree kinship better
  // we estimate sigma2_g * K, but a formal defiinte is 2 * sigma2_g *K,
  // so multiply 0.5 to scale it.
  const double sigma2_g = model.GetSigmaG2() * 0.5;
  const double sigma2_e = model.GetSigmaE2();
  const double herit = (sigma2_g + sigma2_e == 0.) ? 0 : sigma2_g / (sigma2_g + sigma2_e);

  fp->printf("#Sigma2_g\t%g\n", sigma2_g);
  fp->printf("#Sigma2_e\t%g\n", sigma2_e);
  fp->printf("#Heritability\t%g\n", herit);
}

#endif

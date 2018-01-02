#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <Python.h>
#include <numpy/arrayobject.h>

#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "src/proc/proc.h"
#include "src/util/exception.h"
#include "src/5mer/5mer_index.h"
#include "lib/wavelib/header/wavelib.h"
#include <malloc.h>
#include <cmath>
#include <iomanip>

using namespace std;
using namespace g::proc;

//-------- utility ------//
void getBaseName(string &in,string &out,char slash,char dot)
{
	int i,j;
	int len=(int)in.length();
	for(i=len-1;i>=0;i--)
	{
		if(in[i]==slash)break;
	}
	i++;
	for(j=len-1;j>=0;j--)
	{
		if(in[j]==dot)break;
	}
	if(j==-1)j=len;
	out=in.substr(i,j-i);
}
void getRootName(string &in,string &out,char slash)
{
	int i;
	int len=(int)in.length();
	for(i=len-1;i>=0;i--)
	{
		if(in[i]==slash)break;
	}
	if(i<=0)out=".";
	else out=in.substr(0,i);
}
//---------------------- utility ------//over



//--------------- continuous wavelet transform (CWT) analysis -----------------//
/** @scale0: level0 pyramind scale;  @dscale: scale_i = scale0*(2^{i*dsacle} ); @npyr: total number of pyramind*/
void CWTAnalysis(const std::vector<double>& raw, std::vector<std::vector<double> >& output, double scale0, double dscale, long npyr)
{
	const double* sigs = &raw[0];		//sst_nino3.dat
	cwt_object wt;

	long N = raw.size();
	double dt = 1;//2;		//sample rate	>  maybe we should use 2?

	wt = cwt_init("dog", 2.0, N, dt, npyr);	//"morlet", "dog", "paul"
	setCWTScales(wt, scale0, dscale, "pow", 2.0);
	cwt(wt, sigs);

	output.resize(npyr);
	for(long k = npyr; k--;){
		long idx = npyr-k-1;
		output[idx].resize(raw.size());
		long offset = k*raw.size();
		for(long i = 0; i < output[idx].size(); i++){
			output[idx][i] = wt->output[i+offset].re;
		}
	}
	
	cwt_free(wt);
}

//----------------- boundary generation for constrained dynamic time warping (cDTW) -------------//
void BoundGeneration(std::vector<std::pair<long,long> >& cosali, 
	int neib, std::vector<std::pair<long,long> >& bound, int mode)
{

//if(mode!=-1) //-> mode = -1 means Renmin mode
vector<pair<long,long> > cosali_=cosali;
if(mode==1) //-> use partial-diagonol alignment
{

	//-------- generate partial-diagonol alignment -------//
	vector<pair<long,long> > cosali2;
        cosali2.push_back(cosali[0]);
        for(long i = 1; i < cosali.size(); i++){
                if(cosali[i].first != cosali2[cosali2.size()-1].first){
                        cosali2.push_back(cosali[i]);
                }
                else{
                        cosali2[cosali2.size()-1].second = cosali[i].second;
                }
        }

	cosali_.clear();
	for(long i = 1; i < cosali2.size(); i++)
	{
		long pre_idx = cosali2[i-1].first, cur_idx = cosali2[i].first;
		long pre_anchor = cosali2[i-1].second, cur_anchor = cosali2[i].second;
		double anchor_diff = (cur_anchor-pre_anchor)/(cur_idx-pre_idx);
		for(long k = pre_idx, count = 0; k < cur_idx; k++, count++)
		{
			long mid = pre_anchor+(long)(count*anchor_diff);  //assign point relationship
			if(mid<pre_anchor)mid=pre_anchor;
			if(mid>cur_anchor)mid=cur_anchor;
			cosali_.push_back(make_pair(k,mid));
		}
	}
	cosali_.push_back(cosali2[cosali2.size()-1]);
}

	//----> output pre-bound alignnment -------
//	{
//		for(int i = 0; i < cosali_.size(); i++)
//		{
//			printf("%d -> %d  %d \n",i,cosali_[i].first,cosali_[i].second);
//		}
//	}


	//---------- use block bound ------------//start
	//-> get signal length
	long moln1=cosali_[cosali_.size()-1].first+1;
	long moln2=cosali_[cosali_.size()-1].second+1;
	//-> renmin align to sheng style
	std::vector<std::pair<long,long> > align_sheng;
	Renmin_To_Sheng_align(moln1,moln2,cosali_,align_sheng);
	//-> get bound in sheng style
	std::vector<std::pair<long,long> > bound_sheng;
	From_Align_Get_Bound(moln1,moln2,align_sheng,bound_sheng,neib);
	//-> transfer bound to renmin style
	Sheng_To_Renmin_bound(moln1,moln2,bound_sheng,bound);
	//----- maybe useful -----//
	bound[0].first = 0;
	bound[bound.size()-1].first = bound[bound.size()-2].first;
	bound[bound.size()-1].second = cosali[cosali.size()-1].second;



	//----> output post-bound alignnment -------
//	{
//		for(int i = 0; i < bound.size(); i++)
//		{
//			printf("%d -> %d  %d \n",i,bound[i].first,bound[i].second);
//		}
//		exit(-1);
//	}

	return;
	//---------- use block bound ------------//over

/*
	int radius=neib1;
	std::vector<std::pair<int,int> > cosali2;
	
	cosali2.push_back(cosali[0]);
	for(int i = 1; i < cosali.size(); i++){
		if(cosali[i].first != cosali2[cosali2.size()-1].first){
			cosali2.push_back(cosali[i]);
		}
		else{
			cosali2[cosali2.size()-1].second = cosali[i].second;
		}
	}
	
	cosali = cosali2;
	
	bound.resize(cosali[cosali.size()-1].first+1);
	bound.assign(cosali[cosali.size()-1].first+1, std::make_pair(-1,-1));
	
	for(int i = 1; i < cosali.size(); i++){
		int pre_idx = cosali[i-1].first, cur_idx = cosali[i].first;
		int pre_anchor = cosali[i-1].second, cur_anchor = cosali[i].second;
		
		double anchor_diff;
		
		anchor_diff = (cur_anchor-pre_anchor)/(cur_idx-pre_idx);
		
		int neighbor = int(anchor_diff+0.5)+radius;
		
		for(int i = pre_idx, count = 0; i < cur_idx; i++, count++){
			int mid = pre_anchor+std::round(count*anchor_diff);		//assign point relationship
			bound[i].first = mid-neighbor < 0 ? 0 : mid-neighbor;
			bound[i].second = mid+neighbor > cosali[cosali.size()-1].second ? cosali[cosali.size()-1].second : mid+neighbor;
		}
	}
	
// 	for(int i = 0; i < bound.size(); i++){
// 		std::cout<<bound[i].first<<" "<<bound[i].second<<std::endl;
// 	}
	
	bound[0].first = 0;
	bound[bound.size()-1].first = bound[bound.size()-2].first;
	bound[bound.size()-1].second = cosali[cosali.size()-1].second;
*/
}

//====================== FastDTW ===================//
void FastDTW(std::vector<double>& in1, std::vector<double>& in2,
	std::vector<std::pair<long,long> >& alignment, long radius, long max_level, int mode,
	double* totaldiff = 0)
{
	double tdiff;
	vector <vector<pair<long, double> > > sig1peaks, sig2peaks;
	long length1 = in1.size(), length2 = in2.size();
	
	//---- determine level -------//
	long l1=1;
	for(;;)
	{
		long num1=pow(2,l1);
		if(length1/num1<radius+2)break;
		l1++;
	}
	long l2=1;
	for(;;)
	{
		long num2=pow(2,l2);
		if(length2/num2<radius+2)break;
		l2++;
	}
	long level=l1<l2?l1:l2;
	if(level>max_level)level=max_level;

	//---- create sigpeak ------//
	long cur_level=1;
	for(long k=level;k>=0;k--)
	{
		vector<pair<long, double> > sig1peaks_cur;
		vector<pair<long, double> > sig2peaks_cur;

		//------ using Equally Averaging ------//(just for comparison)
		//--> proc peak1
		long cur1=0;
		long num1=pow(2,cur_level);
		for(long i = 0; i < length1/num1 -1; i++)
		{
			double val=0;
			for(long j=0;j<num1;j++)
			{
				val+=in1[cur1];
				cur1++;
			}
			val/=num1;
			sig1peaks_cur.push_back(make_pair(i*num1,val));
		}
		{
			long i=length1/num1-1;
			long cc=0;
			double val=0;
			for(long j=cur1;j<in1.size();j++)
			{
				val+=in1[j];
				cc++;
			}
			val/=cc;
			sig1peaks_cur.push_back(make_pair(in1.size()-1,val));
		}
		//--> proc peak2
		long cur2=0;
		long num2=pow(2,cur_level);
		for(long i = 0; i < length2/num2 -1; i++)
		{
			double val=0;
			for(long j=0;j<num2;j++)
			{
				val+=in2[cur2];
				cur2++;
			}
			val/=num2;
			sig2peaks_cur.push_back(make_pair(i*num2,val));
		}
		{
			long i=length2/num2-1;
			long cc=0;
			double val=0;
			for(long j=cur2;j<in2.size();j++)
			{
				val+=in2[j];
				cc++;
			}
			val/=cc;
			sig2peaks_cur.push_back(make_pair(in2.size()-1,val));
		}

		//----- push_back -----//
		sig1peaks.push_back(sig1peaks_cur);
		sig2peaks.push_back(sig2peaks_cur);
		cur_level++;
	}

	//----------- perform pyamid DTW ----------------//
	for(long k=0;k<level;k++)
	{
		//--- extract peak ----//
		vector<double> peak1(sig1peaks[k].size());
		vector<double> peak2(sig2peaks[k].size());
		for(long i = sig1peaks[k].size(); i--;){
			peak1[i] = sig1peaks[k][i].second;
		}
		for(long i = sig2peaks[k].size(); i--;){
			peak2[i] = sig2peaks[k][i].second;
		}
		
		//--- perform align ----//
		//----- apply DTW or cDTW dependent on k-th level -------//
		if(k == 0){
			tdiff = g::proc::DynamicTimeWarping(peak1, peak2, alignment);
		}
		else{
			//----- ReMapIndex_partI (map ground level upon k-th level) -----//
			long c = 0;
			for(long i = 0; i < alignment.size(); i++){
				while(sig1peaks[k][c].first < alignment[i].first){
					c++;
				}
				alignment[i].first = c;	
			}
			
			long d = 0;
			for(long i = 0; i < alignment.size(); i++){
				while(sig2peaks[k][d].first < alignment[i].second){
					d++;
				}
				alignment[i].second = d;	
			}
			//----- cDWT (constrained DWT) -------//
			vector<pair<long,long> > bound;
			BoundGeneration(alignment, radius, bound, mode);
			tdiff = g::proc::BoundDynamicTimeWarping(peak1, peak2, bound, alignment);
		}
		//----- ReMapIndex_partII (map k-th level back to ground level) -----//
		for(long i = alignment.size(); i--;){
			alignment[i].first = sig1peaks[k][alignment[i].first].first;
			alignment[i].second = sig2peaks[k][alignment[i].second].first;
		}
	}

	//----- return diff ------//
	if(totaldiff){
		*totaldiff = tdiff;
	}
}


//====================== continous wavelet dynamic time warping (cwDTW) ========================//
void MultiLevel_WaveletDTW(std::vector<double>& in1, std::vector<double>& in2,
	std::vector<std::vector<double> >& sig1, std::vector<std::vector<double> >& sig2, 
	std::vector<std::pair<long,long> >& alignment, long radius, int test, int mode, 
	double* totaldiff = 0)
{
	double tdiff;
	std::vector<std::pair<long, double> > sig1peaks, sig2peaks;
	double length1 = sig1[0].size(), length2 = sig2[0].size();

	long tot_size=sig1.size();
	for(long k = 0; k < tot_size; k++)
	{

//printf("k=%d\n",k);

		//------ peakpick CWT signal -------------//
		g::proc::PeakPick(sig1[k], sig1peaks);
		g::proc::PeakPick(sig2[k], sig2peaks);
//		std::cout<<sig1peaks.size()<<"\t"<<sig2peaks.size()<<std::endl;
		std::vector<double> peak1(sig1peaks.size());
		std::vector<double> peak2(sig2peaks.size());


		//-------- use peak picking result ---------//
		for(long i = sig1peaks.size(); i--;){
			peak1[i] = sig1peaks[i].second;
		}
		for(long i = sig2peaks.size(); i--;){
			peak2[i] = sig2peaks[i].second;
		}


//================================ peak average =====================//start
if(test==2)  //-> peak_ave
{
		//-------- use peak average reso --------------//(just for comparison)
		for(long i = 1; i < sig1peaks.size()-1; i++)
		{
			long start=(sig1peaks[i-1].first+sig1peaks[i].first)/2;
			long end=(sig1peaks[i].first+sig1peaks[i+1].first)/2;
			double val=0;
			long cc;
			for(long j=start;j<=end;j++)
			{
				val+=in1[j];
				cc++;
			}
			peak1[i]=val;
		}
		for(long i = 1; i < sig2peaks.size()-1; i++)
		{
			long start=(sig2peaks[i-1].first+sig2peaks[i].first)/2;
			long end=(sig2peaks[i].first+sig2peaks[i+1].first)/2;
			double val=0;
			long cc;
			for(long j=start;j<=end;j++)
			{
				val+=in2[j];
				cc++;
			}
			peak2[i]=val;
		}
}
//================================ peak average =====================//over


//================================ equal average =====================//start
if(test==1)  //-> equal_ave
{
	//------ using Equally Averaging ------//(just for comparison)
	//--> proc peak1
	long cur1=0;
	long num1=(long)(1.0*in1.size()/sig1peaks.size() );
	for(long i = 0; i < sig1peaks.size()-1; i++)
	{
		double val=0;
		for(long j=0;j<num1;j++)
		{
			val+=in1[cur1];
			cur1++;
		}
		val/=num1;
		sig1peaks[i].first=i*num1;
//		sig1peaks[i].first=i*num1+num1/2 < in1.size()-1? i*num1+num1/2:in1.size()-1 ;
//		sig1peaks[i].second=val;
		peak1[i]=val;
	}
	{
		long i=sig1peaks.size()-1;
		long cc=0;
		double val=0;
		for(long j=cur1;j<in1.size();j++)
		{
			val+=in1[j];
			cc++;
		}
		val/=cc;
		sig1peaks[i].first=in1.size()-1;
//		sig1peaks[i].first=i*num1+num1/2 < in1.size()-1? i*num1+num1/2:in1.size()-1 ;
//		sig1peaks[i].second=val;
		peak1[i]=val;
	}
	//--> proc peak2
	long cur2=0;
	long num2=(long)(1.0*in2.size()/sig2peaks.size() );
	for(long i = 0; i < sig2peaks.size()-1; i++)
	{
		double val=0;
		for(long j=0;j<num2;j++)
		{
			val+=in2[cur2];
			cur2++;
		}
		val/=num2;
		sig2peaks[i].first=i*num2;
//		sig2peaks[i].first=i*num2+num2/2 < in2.size()-1? i*num2+num2/2:in2.size()-1 ;
//		sig2peaks[i].second=val;
		peak2[i]=val;
	}
	{
		long i=sig2peaks.size()-1;
		long cc=0;
		double val=0;
		for(long j=cur2;j<in2.size();j++)
		{
			val+=in2[j];
			cc++;
		}
		val/=cc;
		sig2peaks[i].first=in2.size()-1;
//		sig2peaks[i].first=i*num2+num2/2 < in2.size()-1? i*num2+num2/2:in2.size()-1 ;
//		sig2peaks[i].second=val;
		peak2[i]=val;
	}
}
//================================ equal average =====================//over

		//----- apply DTW or cDTW dependent on k-th level -------//
		if(k == 0){
			tdiff = g::proc::DynamicTimeWarping(peak1, peak2, alignment);
		}
		else{
			//----- ReMapIndex_partI (map ground level upon k-th level) -----//
			long c = 0;
			for(long i = 0; i < alignment.size(); i++){
				while(sig1peaks[c].first < alignment[i].first){
					c++;
				}
				alignment[i].first = c;	
			}
			
			long d = 0;
			for(long i = 0; i < alignment.size(); i++){
				while(sig2peaks[d].first < alignment[i].second){
					d++;
				}
				alignment[i].second = d;	
			}
			//----- cDWT (constrained DWT) -------//
			std::vector<std::pair<long,long> > bound;
			long neib=radius*pow(2,tot_size-k); //adaptive radius
			BoundGeneration(alignment, neib, bound, mode);
			tdiff = g::proc::BoundDynamicTimeWarping(peak1, peak2, bound, alignment);
		}
		//----- ReMapIndex_partII (map k-th level back to ground level) -----//
		for(long i = alignment.size(); i--;){
			alignment[i].first = sig1peaks[alignment[i].first].first;
			alignment[i].second = sig2peaks[alignment[i].second].first;
		}

	}
	
	if(totaldiff){
		*totaldiff = tdiff;
	}
}

//----------- write alignment to file -------------//__2017.10.15__//(Sheng modified)
void WriteSequenceAlignment(const char* output, 
	const std::vector<double>& reference_orig, const std::vector<double>& peer_orig,
	const std::vector<double>& reference, const std::vector<double>& peer,
	vector<pair<long,long> >& alignment, int swap)
{
	vector <std::string> tmp_rec;
	double diff;	
	for(long i = 0; i < alignment.size(); i++)
	{
		//----- output to string ----//
		std::ostringstream o;
		diff = std::fabs(reference[alignment[i].first]-peer[alignment[i].second]);
		if(swap==1)
		{
			o<<setw(10)<<alignment[i].second+1<<" "<<setw(10)<<alignment[i].first+1<<" | ";
			o<<setw(15)<<reference_orig[alignment[i].second]<<" "<<setw(15)<<peer_orig[alignment[i].first]<<" | ";
			o<<setw(15)<<peer[alignment[i].second]<<" "<<setw(15)<<reference[alignment[i].first];
		}
		else
		{
			o<<setw(10)<<alignment[i].first+1<<" "<<setw(10)<<alignment[i].second+1<<" | ";
			o<<setw(15)<<reference_orig[alignment[i].first]<<" "<<setw(15)<<peer_orig[alignment[i].second]<<" | ";
			o<<setw(15)<<reference[alignment[i].first]<<" "<<setw(15)<<peer[alignment[i].second];
		}
		o<<"          diff:"<<setw(15)<<diff;
		//----- record string -----//
		std::string s=o.str();
		tmp_rec.push_back(s);
	}
	//----- output to file ------//
	FILE *fp=fopen(output,"wb");
	for(long i=0;i<(long)tmp_rec.size();i++)fprintf(fp,"%s\n",tmp_rec[i].c_str());
	fclose(fp);
}


static PyObject* cwdtw_wrapper(PyObject * self, PyObject * args) {

  // query and target array data and lengths
  PyArrayObject *target_array = (PyArrayObject*)PyTuple_GetItem(&args[0], 0);

  // check array type - this is important
  PyArray_Descr *d = PyArray_DESCR(target_array);
  if((*d).kind != 'f' || (*d).elsize != 4) {
    printf("Target array dtype is %c%d, should be f4\n", (*d).kind, (*d).elsize);
    Py_RETURN_NONE;
  }

  float *target = (float*)PyArray_DATA(target_array);
  uint32_t tlen = PyArray_DIMS(target_array)[0];
  std::vector<double> target_vector (target, target + tlen);

  PyArrayObject *query_array = (PyArrayObject*)PyTuple_GetItem(&args[0], 1);

  // check array type - this is important
  d = PyArray_DESCR(query_array);
  if((*d).kind != 'f' || (*d).elsize != 4) {
    printf("Query array dtype is %c%d, should be f4\n", (*d).kind, (*d).elsize);
    Py_RETURN_NONE;
  }

  float *query = (float*)PyArray_DATA(query_array);
  uint32_t qlen = PyArray_DIMS(query_array)[0];
  std::vector<double> query_vector (query, query + qlen);

  int band_size = PyLong_AsLong(PyTuple_GetItem(&args[0], 2));


  // ------ from cwDTW main() -----
  
	//==================================================//
	//------3. process initial input signals ----------//

	//----- 3.1 Zscore normaliza on both signals -----//
	g::proc::ZScoreNormalize(query_vector);
	g::proc::ZScoreNormalize(target_vector);


	//====================================================//
	//----- 4. continous wavelet transform --------------//
	std::vector<std::vector<double> > rcwt, pcwt;
	
	long npyr = (long)band_size;          // default: 3
	double scale0 = sqrt(2);	// default: sqrt(2)
	double dscale = 1;              // default: 1

	CWTAnalysis(query_vector, rcwt, scale0, dscale, npyr);	
	CWTAnalysis(target_vector, pcwt, scale0, dscale, npyr);

	//------ 4.1 Zscore normaliza on both CWT signals -----//	
	//if multiscale is used, pyr logical should be added.
	for(long i = 0; i < npyr; i++){
		g::proc::ZScoreNormalize(rcwt[i]);
		g::proc::ZScoreNormalize(pcwt[i]);
	}


	//============================================//
	//------ 5. multi-level WaveletDTW ----------//
	std::vector<std::pair<long,long> > cosali;
	double tdiff;

	MultiLevel_WaveletDTW(query_vector, target_vector, rcwt, pcwt, cosali, band_size, 0, 0, &tdiff);

	//------ 5.1 generate final boundary -------//
	std::vector<std::pair<long,long> > bound;
	BoundGeneration(cosali, band_size, bound, 0);

	//------ 5.2 generate final alignment via cDTW ------//
	std::vector<std::pair<long,long> > alignment;
	tdiff = g::proc::BoundDynamicTimeWarping(query_vector, target_vector, bound, alignment);


  PyObject* ret = PyList_New(6);
  PyList_SetItem(ret, 0, (PyObject*)PyFloat_FromDouble(tdiff));
  PyList_SetItem(ret, 1, (PyObject*)PyLong_FromLong(alignment.size()));
  PyList_SetItem(ret, 2, (PyObject*)PyLong_FromLong(alignment[0].first));
  PyList_SetItem(ret, 3, (PyObject*)PyLong_FromLong(alignment[alignment.size()-1].first));
  PyList_SetItem(ret, 4, (PyObject*)PyLong_FromLong(alignment[0].second));
  PyList_SetItem(ret, 5, (PyObject*)PyLong_FromLong(alignment[alignment.size()-1].second));

  return ret;
}


// ------ Python 2 and 3 support, as per https://docs.python.org/3/howto/cporting.html ------

struct module_state {
    PyObject *error;
};

#if PY_MAJOR_VERSION >= 3
#define GETSTATE(m) ((struct module_state*)PyModule_GetState(m))
#else
#define GETSTATE(m) (&_state)
static struct module_state _state;
#endif

static PyObject *
error_out(PyObject *m) {
    struct module_state *st = GETSTATE(m);
    PyErr_SetString(st->error, "something bad happened");
    return NULL;
}

static PyMethodDef module_methods[] = {
    {"error_out", (PyCFunction)error_out, METH_NOARGS, NULL},
    {"cwDTW", (PyCFunction)cwdtw_wrapper, METH_VARARGS, "cwDTW"},
    {NULL, NULL}
};

#if PY_MAJOR_VERSION >= 3

static int myextension_traverse(PyObject *m, visitproc visit, void *arg) {
    Py_VISIT(GETSTATE(m)->error);
    return 0;
}

static int myextension_clear(PyObject *m) {
    Py_CLEAR(GETSTATE(m)->error);
    return 0;
}


static struct PyModuleDef moduledef = {
  PyModuleDef_HEAD_INIT,
  "cwdtw",
  NULL,
  sizeof(struct module_state),
  module_methods,
  NULL,
  myextension_traverse,
  myextension_clear,
  NULL
};

#define INITERROR return NULL

PyMODINIT_FUNC
PyInit_cwdtw(void)

#else
#define INITERROR return

void
PyInit_cwdtw(void)
#endif
{
#if PY_MAJOR_VERSION >= 3
  PyObject *module = PyModule_Create(&moduledef);
#else
  PyObject *module = Py_InitModule("cwdtw", module_methods, "Python-wrapped cwDTW module");
#endif

  if (module == NULL)
    INITERROR;
  struct module_state *st = GETSTATE(module);

  st->error = PyErr_NewException("myextension.Error", NULL, NULL);
  if (st->error == NULL) {
    Py_DECREF(module);
    INITERROR;
  }

#if PY_MAJOR_VERSION >= 3
  return module;
#endif
}

/*
 * ---------------------------------------------------------------------------------------------
 */

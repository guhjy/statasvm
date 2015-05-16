
#include <stdlib.h>
#include <stdio.h>
#include <string.h> //..yes.. there are two string headers
#include <strings.h>
#include <stdbool.h>
#include <math.h> //for NAN

#include <svm.h>
#include "stplugin.h"


#define PLUGIN_NAME "_svm"

#if _WIN32
// MS is not standard C, of course:
// https://msdn.microsoft.com/en-us/library/2ts7cx93.aspx
#define snprintf _snprintf
#endif


void svm_problem_pprint(struct svm_problem* prob) {
  for(int i=0; i<prob->l; i++) {
    printf("y:%lf,", prob->y[i]);
    for(int j=0; prob->x[i][j].index != -1; j++) {
      printf(" %d:%lf", prob->x[i][j].index, prob->x[i][j].value);
    }
    printf("\n");
  }
}


void svm_parameter_pprint(struct svm_parameter* param) {
	printf("svm_parameter %p:\n", param);
	
	printf("\tsvm_type: %d\n", param->svm_type); //TODO: map to enum names
	printf("\tkernel_type: %d\n", param->kernel_type);
	printf("\tdegree: %d\n", param->degree);
	
	printf("\tgamma: %lf\n", param->gamma);
	printf("\tcoef0: %lf \n", param->coef0);
	
	printf("\tcache_size: %lf\n", param->cache_size);
	printf("\teps: %lf \n", param->eps);
	printf("\tC: %lf\n", param->C);
	
	printf("\tnr_weight: %d\n", param->nr_weight);
	printf("\tweight_label @ %p: TODO \n", param->weight_label/*[i] for i in range smethingsomething*/);
	printf("\tweight @ %p: TODO \n", param->weight); //
	
	printf("\tnu: %lf\n", param->nu);
	printf("\tp: %lf\n", param->p);
	
	printf("\tshrinking: %d\n", param->shrinking); //TODO: these are booleans, not ints
	printf("\tprobability: %d\n", param->probability);
}

/* 
 * free an svm_problem structure.
 * this assumes that entries in prob.x[] have been individually allocated
 * which is not something currently guaranteed by libsvm
 * (just see their bundled svm-train.c's use of 'x_space' for a quick and therefore dirty approach).
 * it also assumes that the svm_problem itself is allocated on the heap,
 *   again is not guaranteed by libsvm.
 */
void svm_problem_free(struct svm_problem* prob) {
  for(int i=0; i<prob->l; i++) {
    free(prob->x[i]);
  }
	free(prob->y);
	free(prob->x);
	free(prob);
}

/* Helper routine for reading svmlight files into Stata.
 * We adopt the libsvm people's method: scan the data twice.
 *
 * Stata doesn't let C plugins add new variables, but parsing with gettoken is agonizingly slow
 * so there is this ugly marriage:
 *   i. find out the size (svmlight_read("pre", filename))
 *  ii. return this to Stata
 * iii. Stata edits the data table, allocating space
 *  iv. read in the data (svmlight_read(filename))
 *
 * Yes, it is at least an order of magnitude faster to read the data twice in C,
 * even with the interlude back to Stata, than to do one pass with gettoken (Stata's wrapper around strtok())
 *
 * To reduce code, both scans are handled by this one function (named to not conflict with the built in POSIX read())
 * and its behaviour is modified with a flag of "pre" given to indicate the preread phase i.
 * The results of the preread phase are passed back via Stata scalars
 *   N (the number of observations) and
 *   M (the number of 'features', i.e. the number of variables except for the first Y variable);
 * Stata's C interface doesn't (apparently) provide any way to use tempnam or the r() table.4
 *   but Stata scalars are in a single global namespace, so to avoid naming conflicts we prefix the N and M by the name of this function. 
 *
 * Special case: the tag on an X variable (a 'feature') could also be the special words
 * "sid:" or "cost:" (slack and weighting tweaks, respectively), according to the svmlight source.
 * libsvm does not support these so for simplicity neither do we.
 *
 * TODO:
 * [ ] better errors messages (using an err buf or by defining a better SF_error())
 * [x] check for off-by-ones
 * [ ] format conformity:
 *   - libsvm gets annoyed if features are given out of order
 *   - libsvm will accept feature id 0, though none of the. Perhaps we should also pass back a *minimum*?
 *   - how does svm_light compare?
 */
STDLL svmlight_read(int argc, char* argv[]) {
  
  ST_retcode err = 0;
  
  bool reading = true;
  
  if(argc > 1) {
		char* subcmd = argv[0];
		argc--; argv++;
		if(strncmp(subcmd, "pre", 5) == 0) {
			reading = false;
		} else {		
		  SF_error("Unrecognized read subcommand %s\n"/* subcmd*/);
		  return 1;
		}
	}
	
	if(argc != 1) {
    SF_error("Wrong number of arguments\n");
    return 1;
  }
  
  
#if DEBUG
	printf("svm read");
	if(!reading) {
	  printf(" pre");
	}
	printf("\n");
#endif
  
  char* fname = argv[0];
  FILE* fd = fopen(fname, "r");
  if(fd == NULL) {
    SF_error("Unable to open file\n");
    return 1;
  }
  
  int M = 0, N = 0;
  
  // N is the number of lines
  // M is the *maximum* number of features in a single line
  	
	double y;
	
	long int id;
	double x;
	
	// we tree the svmlight format as a sequence of space-separated tokens, which is really easy to do with scanf(),
	// where some of the tokens are
	//   single floats (marking a new observation) and some are
	//   pairs feature_id:value (giving a feature value)
	// scanf is tricky, but it's the right tool for this job: parsing sequences of ascii numbers.
	// TODO: this parser is *not quite conformant* to the non-standard:
	//   it will treat two joined svmlight lines as two separate ones (that is, 1 3:4 5:6 2 3:9 will be two lines with classes '1' and '2' instead of an error; it should probably be an error)
	char* tok;
  while(fscanf(fd, "%ms", &tok) == 1) {
    //printf("read token=|%s|\n", tok); //DEBUG
    if(sscanf(tok, "%ld:%lf", &id, &x) == 2) { //this is a more specific match than the y match so it must come first
  	  if(id < 1) {
  			SF_error("parse error: only positive feature IDs allowed\n");
  			return 4;
  		}
  		if(M < id) M = id;
  		
	    if(reading) {
#if DEBUG
				//printf("storing to X[%d,%ld]=%lf; X is %dx%d\n", N, id, x, SF_nobs(), SF_nvar()-1); //DEBUG
#endif
		    SF_vstore(id+1 /*stata counts from 1, so y has index 1, x1 has index 2, ... , x7 has index 8 ...*/, N, x);
			  if(err) {
#if DEBUG
			    SF_error("unable to store to x\n");
#endif
			    return err;
			  }
	    }
    } else if(sscanf(tok, "%lf", &y) == 1) { //this should get the first 'y' value
      
      N+=1;
      
	    if(reading) {
#if DEBUG
				printf("storing to Y[%d]=%lf; Y is %dx1.\n", N, y, SF_nobs()); //DEBUG
#endif
			  err = SF_vstore(1 /*stata counts from 1*/, N, y);
			  if(err) {
#if DEBUG
			    printf("unable to store to y\n");
#endif
			    SF_error("unable to store to y\n");
			    return err;
		    }
	    }
	    
    } else {
    	SF_error("parse error\n");
      return 1;
    }	
  }
	
  
#if DEBUG
	if(!reading) {
	  printf("svm read pre: total dataset is %dx(1+%d)\n", N, M);
	}
#endif
  
  if(!reading) {
    // return the preread mode results, but only in preread mode
    err = SF_scal_save("_svm_load_N", (ST_double)N);
    if(err) {
      SF_error("Unable to export scalar 'N' to Stata\n");
      return err;
    }
    
    err = SF_scal_save("_svm_load_M", (ST_double)M);
    if(err) {
      SF_error("Unable to export scalar 'N' to Stata\n");
      return err;
    }
  }
  return 0;
}





/* 
 * convert the Stata varlist format to libsvm sparse format
 * takes no arguments because the varlist is an implicit global (from stplugin.h)
 * as is Stata-standard, the first variable is the regressor Y, the rest are regressees (aka features) X
 * The result is a svm_problem, which is essentially just two arrays:
 * the Y vector and the X design matrix. See svm.h for details.
 *
 * Note that this will mangle the indecies: the libsvm indecies will start from 1, mapping *whichever variable was 2nd given in varlist to 1, 3rd to 2, ...*.
 * If your workflow is "svm load using file.svmlight; svm train *" then there will be no mangling, but if you instead say "svm train y x3 x9" then 3 [Stata]->1 [libsvm] and 9 [Stata]->2 [libsvm]
 * this is acceptable since the libsvm indices are ephemeral, never exposed to the Stata user, it just makes debugging confusing.
 *
 * caller is responsible for freeing the result.
 * 
 * Compare sklearn's [dense2libsvm](TODO), which does the same job but coming from a numpy matrix instead 
 */
struct svm_problem* stata2libsvm() {
  struct svm_problem* prob = malloc(sizeof(struct svm_problem));
  bzero(prob, sizeof(struct svm_problem)); //zap the memory just in case
  
  prob->l = 0; //initialize the number of observations
  // we cannot simply malloc into mordor, because `if' and `in' cull what's available
  // so what we really need is a dynamic array
  // for now, in lieu of importing a datastructure to handle this, I'll do it with realloc
  int capacity = 1;
  prob->y = malloc(sizeof(*(prob->y))*capacity);
  if(prob->y == NULL) {
    //TODO: error
    goto cleanup;
  }
  prob->x = malloc(sizeof(*(prob->x))*capacity);
  if(prob->x == NULL) {
    //TODO: error
    goto cleanup;
  }
  
  //TODO: double-check this for off-by-one bugs
  // This code is super confusing because we're dealing with three numbering systems: C's 0-based, Stata's 1-based, and libsvm's (.index) which is 0-based but sparse
  
	for(ST_int i = SF_in1(); i <= SF_in2(); i++) { //respect `in' option
		if(SF_ifobs(i)) {			    									 //respect `if' option
			if(prob->l >= capacity) {	// amortized-resizing
				capacity<<=1; //double the capacity
				prob->y = realloc(prob->y, sizeof(*(prob->y))*capacity);
				prob->x = realloc(prob->x, sizeof(*(prob->x))*capacity);
			}
			
			// put data into Y[l]
			// (there is only one Y so we hard-code the variable index)
			SF_vdata(1, prob->l+1, &(prob->y[prob->l]));
			
			// put data into X[l]
			// (there are many values)
      prob->x[prob->l] = calloc(SF_nvars(), sizeof(struct svm_node)); //TODO: make these inner arrays also dynamically grow. for a sparse problem this will drastically overallocate. (though I suppose your real bottleneck will be Stata, which doesn't do sparse)
      if(prob->x[prob->l] == NULL) {
        goto cleanup;
      }
      
			// libsvm uses a sparse datastructure
			// that means that missing values should not be allocated
			// the length of each row is indicated by index=-1 on the last entry
			int c = 0; //and the current position within the subarray is tracked here
			for(int j=1; j<SF_nvars(); j++) {
				ST_double value;
				if(SF_vdata(j+1 /*this +1 accounts for the y variable: variable 2 in the Stata dataset is x1 */, prob->l+1, &value) == 0 && !SF_is_missing(value)) {
					prob->x[prob->l][c].index = j;
					prob->x[prob->l][c].value = value;
					c++;
				}
      prob->x[prob->l][c].index = -1; //mark end-of-row
      prob->x[prob->l][c].value = SV_missval; //not strictly necessary, but it makes me feel good
			}
			prob->l++;
		}
	}

  //return overallocated memory by downsizing
	prob->y = realloc(prob->y, sizeof(*(prob->y))*prob->l);
	prob->x = realloc(prob->x, sizeof(*(prob->x))*prob->l);
	
	return prob;
	
cleanup:
  SF_error("stata2libsvm failed\n");
  //TODO: clean up after ourselves
	//TODO: be careful to check the last entry for partially initialized
	return NULL;
}


STDLL train(int argc, char* argv[]) {
	
	struct svm_parameter param;
	// set up svm_paramet default values
	
	// TODO: pass (probably name=value pairs on the "command line")
	param.svm_type = C_SVC;
	param.kernel_type = RBF;
	param.degree = 3;
	param.gamma = 0;	// 1/num_features
	param.coef0 = 0;
	param.nu = 0.5;
	param.cache_size = 100;
	param.C = 1;
	param.eps = 1e-3;
	param.p = 0.1;
	param.shrinking = 1;
	param.probability = 0;
	param.nr_weight = 0;
	param.weight_label = NULL;
	param.weight = NULL;
	
	struct svm_problem* prob = stata2libsvm();
  if(prob == NULL) {
    SF_error("stata2libsvm failed\n");
    return 1;
  }
#if DEBUG
  svm_problem_pprint(prob);
  svm_parameter_pprint(&param);
#endif

  const char *error_msg = NULL;
	error_msg = svm_check_parameter(prob,&param);
	if(error_msg) {
		char error_buf[256];
		snprintf(error_buf, 256, "Parameter error: %s", error_msg);
		SF_error((char*)error_buf);
		return(1);
	}
	
	struct svm_model* model = svm_train(prob,&param); //a 'model' in libsvm is what I would call a 'fit' (I would call the structure being fitted to---svm---the model), but beggars can't be choosers
	(void)model; //silence unused-variable warnings temporarily

#if DEBUG
	if(svm_save_model("DEBUG.model", model)) {
		SF_error("DEBUG ERROR: unable to export fitted model\n");
	}
#endif
	svm_destroy_param(&param); //the model copies 'param' into itself, so we should free it here
	svm_problem_free(prob);
	
	//svm_free_and_destroy_model(&model); //XXX when should this happen? this should get stored so we can call predict() on it. does 'program drop _svm' autofree all memory too?
  // should 'model' be a global?????? that's so against my training, but it's also sort of how Stata rolls.
  
  return 0;
}





void print_stata(const char* s) {
  SF_display((char*)s);
}

#if HAVE_SVM_PRINT_ERROR
// TODO: libsvm doesn't have a svm_set_error_string_function, but if I get it added this is the stub
void error_stata(const char* s) {
  SF_error((char*)s);
}
#endif


/* Initialization code adapted from
    stplugin.c, version 2.0
    copyright (c) 2003, 2006        			StataCorp
 */ 
ST_plugin *_stata_ ;

STDLL pginit(ST_plugin *p)
{
	_stata_ = p ;
	
	svm_set_print_string_function(print_stata);
#if HAVE_SVM_PRINT_ERROR
	svm_set_error_string_function(error_stata);
#endif
	
	return(SD_PLUGINVER) ;
}



/* Stata only lets an extension module export a single function (which I guess is modelled after each .ado file being a single function, a tradition Matlab embraced as well)
 * to support multiple routines the proper way we would have to build multiple DLLs, and to pass variables between them we'd have to figure out
 * instead of fighting with that, I'm using a tried and true method: indirection:
 *  the single function we export is a trampoline, and the subcommands array the list of places it can go to
 */
#define COMMAND_MAX 12 //this isn't actually used to enforce storage limits in subcommands (if we say 'const char name[COMMAND_MAX]' then it is impossible to define the last one as NULL, which is a bother, subcommands should respect this
struct {
  const char* name;
  STDLL (*func)(int argc, char* argv[]);
} subcommands[] = {
	{ "read", svmlight_read },
  { "train", train },
  //{ "predict", predict },
  { NULL, NULL }
};

/* the Stata plugin interface is really really really basic:
 * . plugin call var1 var2, op1 op2 77 op3
 * causes argv to contain "op1", "op2", "77", "op3", and
 * /implicitly/, var1 and var2 are available through the macros SF_{nvars,{v,s}{data,store}}().
 *  (The plugin doesn't get to know (?) the names of the variables in `varlist'. [citation needed])
 *
 * The SF_mat_<op>(char* M, ...) macros access matrices in Stata's global namespace, by their global name.
 */
STDLL stata_call(int argc, char *argv[])
{
#if DEBUG
	print_stata("Stata-SVM v0.0.1\n") ;
	for(int i=0; i<argc; i++)
	{
		printf("argv[%d]=%s\n",i,argv[i]);
	}
	
	printf("Total dataset size: %dx%d. We have been asked operate on [%d:%d,%d].\n", SF_nobs(), SF_nvar(), SF_in1(), SF_in2(), SF_nvars());
#endif
	if(argc < 1) {
		SF_error(PLUGIN_NAME ": no subcommand specified\n");
		return(1);
	}
	
	char* command = argv[0];
	argc--; argv++; //shift off the first arg before passing argv to the subcommand
	
	int i = 0;
	while(subcommands[i].name) {
		if(strncmp(command, subcommands[i].name, COMMAND_MAX) == 0) {
			return subcommands[i].func(argc, argv);
		}
		
		i++;
	}
	
	char err_buf[256];
	snprintf(err_buf, 256, PLUGIN_NAME ": unrecognized subcommand '%s'\n", command);
	SF_error(err_buf);
	
	return 1;
}


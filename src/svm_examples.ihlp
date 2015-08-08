{title:Examples: binary classification}

{pstd}Setup{p_end}
{phang2}{cmd:. sysuse auto}{p_end}

{pstd}Machine learning methods like SVM are very easy to overfit.{p_end}
{pstd}To compensate, it is important to split data into training and testing sets, fit on{p_end}
{pstd}the former and measure performance on the latter, so that performance measurements{p_end}
{pstd}are not artificially inflated by data they've already seen.{p_end}

{pstd}But after splitting the proportion of classes can become unbalanced.{p_end}
{pstd}The reliable way to handle this is a stratified split, a split that{p_end}
{pstd}fixes the proportions of each class in each partition of each class.{p_end}
{pstd}The quick and dirty way is a shuffle:{p_end}
{phang2}{cmd:. set seed 9876}{p_end}
{phang2}{cmd:. gen u = uniform()}{p_end}
{phang2}{cmd:. sort u}{p_end}

{pstd}before the actual train/test split:{p_end}
{phang2}{cmd:. local split = floor(_N/2)}{p_end}
{phang2}{cmd:. local train = "1/`=`split'-1'"}{p_end}
{phang2}{cmd:. local test = "`split'/`=_N'"}{p_end}

{pstd}Fit the classification model on the training set, with 'verbose' enabled.{p_end}
{pstd}Training cannot handle missing data; here we elide it, but usually you should impute.{p_end}
{phang2}{cmd:. svm foreign price-gear_ratio if !missing(rep78) in `train', v}{p_end}

{pstd}Predict on the test set.{p_end}
{pstd}Unlike training, predict can handle missing data: it simply predicts missing.{p_end}
{phang2}{cmd:. predict P in `test'}{p_end}

{pstd}Compute error rate: the percentage of mispredictions is the mean of err.{p_end}
{phang2}{cmd:. gen err = foreign != P in `test'}{p_end}
{phang2}{cmd:. sum err in `test'}{p_end}

{pstd}{it:({stata "example svm binary_classification":click to run})}{p_end}

{title:Examples: multiclass classification}

{pstd}Setup{p_end}
{phang2}{cmd:. use attitude_indicators}{p_end}

{pstd}Shuffle{p_end}
{phang2}{cmd:. set seed 4532}{p_end}
{phang2}{cmd:. gen u = uniform()}{p_end}
{phang2}{cmd:. sort u}{p_end}

{pstd}Train/test split{p_end}
{phang2}{cmd:. local split = floor(_N*3/4)}{p_end}
{phang2}{cmd:. local train = "1/`=`split'-1'"}{p_end}
{phang2}{cmd:. local test = "`split'/`=_N'"}{p_end}

{pstd}In general, you need to do grid-search to find good tuning parameters.{p_end}
{pstd}These values of kernel, gamma, and coef0 just happened to be good enough.{p_end}
{phang2}{cmd:. svm attitude q* in `train', kernel(poly) gamma(0.5) coef0(7)}{p_end}

{phang2}{cmd:. predict P in `test'}{p_end}

{pstd}Compute error rate.{p_end}
{phang2}{cmd:. gen err = attitude != P in `test'}{p_end}
{phang2}{cmd:. sum err in `test'}{p_end}

{pstd}An overly high percentage of SVs means overfitting{p_end}
{phang2}{cmd:. di "Percentage that are support vectors: `=round(100*e(N_SV)/e(N),.3)'"}{p_end}

{pstd}{it:({stata "example svm multiclass_classification":click to run})}{p_end}

{title:Examples: class probability}

{pstd}Setup{p_end}
{phang2}{cmd:. use attitude_indicators}{p_end}

{pstd}Shuffle{p_end}
{phang2}{cmd:. set seed 12998}{p_end}
{phang2}{cmd:. gen u = uniform()}{p_end}
{phang2}{cmd:. sort u}{p_end}

{pstd}Train/test split{p_end}
{phang2}{cmd:. local split = floor(_N*3/4)}{p_end}
{phang2}{cmd:. local train = "1/`=`split'-1'"}{p_end}
{phang2}{cmd:. local test = "`split'/`=_N'"}{p_end}

{pstd}Model{p_end}
{phang2}{cmd:. svm attitude q* in `train', kernel(poly) gamma(0.5) coef0(7) prob}{p_end}
{phang2}{cmd:. predict P in `test', prob}{p_end}

{pstd}the value in column P matches the column P_<attitude> with the highest probability{p_end}
{phang2}{cmd:. list attitude P* in `test'}{p_end}

{pstd}Compute error rate.{p_end}
{phang2}{cmd:. gen err = attitude != P in `test'}{p_end}
{phang2}{cmd:. sum err in `test'}{p_end}

{pstd}Beware:{p_end}
{pstd}predict, prob is a *different algorithm* than predict, and can disagree about predictions.{p_end}
{pstd}This disagreement will become absurd if combined with poor tuning.{p_end}
{phang2}{cmd:. predict P2 in `test'}{p_end}
{phang2}{cmd:. gen agree = P == P2 in `test'}{p_end}
{phang2}{cmd:. sum agree in `test'}{p_end}

{pstd}{it:({stata "example svm class_probability":click to run})}{p_end}

{title:Examples: regression}

{pstd}Setup{p_end}
{phang2}{cmd:. webuse highschool}{p_end}

{pstd}Shuffle{p_end}
{phang2}{cmd:. set seed 793742}{p_end}
{phang2}{cmd:. gen u = uniform()}{p_end}
{phang2}{cmd:. sort u}{p_end}

{pstd}Train/test split{p_end}
{phang2}{cmd:. local split = floor(_N/2)}{p_end}
{phang2}{cmd:. local train = "1/`=`split'-1'"}{p_end}
{phang2}{cmd:. local test = "`split'/`=_N'"}{p_end}

{pstd}Regression is invoked with type(epsilon_svr) or type(nu_svr).{p_end}
{pstd}Notice that you can expand factors (categorical predictors) into sets of{p_end}
{pstd}indicator (boolean/dummy) columns with standard i. syntax, and you can{p_end}
{pstd}record which observations were chosen as support vectors with sv().{p_end}
{phang2}{cmd:. svm weight height i.race i.sex in `train', type(epsilon_svr) sv(Is_SV)}{p_end}

{pstd}Examine which observations were SVs. Ideally, a small number of SVs are enough.{p_end}
{phang2}{cmd:. tab Is_SV in `train'}{p_end}

{phang2}{cmd:. predict P in `test'}{p_end}

{pstd}Compute residuals.{p_end}
{phang2}{cmd:. gen res = (weight - P) in `test'}{p_end}
{phang2}{cmd:. sum res}{p_end}

{pstd}{it:({stata "example svm regression":click to run})}{p_end}

{title:Examples: oneclass}

{pstd}Setup{p_end}
{pstd}[....]{p_end}

{pstd}TODO{p_end}
{phang2}{cmd:. svm, type(one_class)}{p_end}

{pstd}{it:({stata "example svm oneclass":click to run})}{p_end}


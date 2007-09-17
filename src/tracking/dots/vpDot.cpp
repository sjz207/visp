/****************************************************************************
 *
 * $Id: vpDot.cpp,v 1.27 2007-09-17 09:18:28 fspindle Exp $
 *
 * Copyright (C) 1998-2006 Inria. All rights reserved.
 *
 * This software was developed at:
 * IRISA/INRIA Rennes
 * Projet Lagadic
 * Campus Universitaire de Beaulieu
 * 35042 Rennes Cedex
 * http://www.irisa.fr/lagadic
 *
 * This file is part of the ViSP toolkit.
 *
 * This file may be distributed under the terms of the Q Public License
 * as defined by Trolltech AS of Norway and appearing in the file
 * LICENSE included in the packaging of this file.
 *
 * Licensees holding valid ViSP Professional Edition licenses may
 * use this file in accordance with the ViSP Commercial License
 * Agreement provided with the Software.
 *
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Contact visp@irisa.fr if any conditions of this licensing are
 * not clear to you.
 *
 * Description:
 * Track a white dot.
 *
 * Authors:
 * Eric Marchand
 * Fabien Spindler
 *
 *****************************************************************************/

/*
  \file vpDot.cpp
  \brief Track a white dot
*/

#include <visp/vpDot.h>

#include <visp/vpDisplay.h>
#include <visp/vpColor.h>

// exception handling
#include <visp/vpTrackingException.h>

/*
  \class vpDot
  \brief Track a white dot
*/

/* spiral size for the dot search */
const unsigned int vpDot::SPIRAL_SEARCH_SIZE = 50;

/*!

  Initialize the tracker with default parameters.
  - connexity is set to 4 (see setConnexity())
  - dot maximal size is set to 25% of the image size (see setMaxDotSize())
*/
void vpDot::init()
{
  cog_u = 0 ;
  cog_v = 0 ;

  cog_ufloat = 0 ;
  cog_vfloat = 0 ;

  compute_moment = false ;
  graphics = false ;
  maxDotSizePercentage = 0.25 ; // 25 % of the image size
  
  mean_gray_level = 0;
  gray_level_min = 128;
  gray_level_max = 255;
  grayLevelPrecision = 0.85;
  gamma = 1.5 ;

  m00 = m11 = m02 = m20 = m10 = m01 = 0 ;

  connexity = CONNEXITY_4;

  u_min = u_max = v_min = v_max = 0;

  gray_level_out = 0;
  nbMaxPoint = 0;
}

vpDot::vpDot() : vpTracker()
{
  init() ;
}

/*!
  \brief constructor with initialization of the dot location

  \param u : dot location (column)
  \param v : dot location (row)
 */
vpDot::vpDot(const unsigned int u, const unsigned int v) : vpTracker()
{
  init() ;

  cog_u = u ;
  cog_v = v ;

  cog_ufloat = u ;
  cog_vfloat = v ;

}

/*!
  \brief constructor with initialization of the dot location

  \param u : dot location (column)
  \param v : dot location (row)
 */
vpDot::vpDot(const double u,const  double v) : vpTracker()
{

  init() ;

  cog_u = (unsigned int)u ;
  cog_v = (unsigned int)v ;

  cog_ufloat = u ;
  cog_vfloat = v ;

}

/*!
  \brief copy constructor
 */
vpDot::vpDot(const vpDot& c)  : vpTracker()
{

  *this = c ;

}


/*!
  \brief destructor
 */
vpDot::~vpDot()
{

  Lu.kill() ;
  Lv.kill() ;

}

/*!
  \brief copy operator
 */
vpDot&
vpDot::operator=(const vpDot& pt)
{
  cog_u = pt.cog_u ;
  cog_v = pt.cog_v ;

  cog_ufloat = pt.cog_ufloat ;
  cog_vfloat = pt.cog_vfloat ;

  graphics = pt.graphics ;
  mean_gray_level = pt.mean_gray_level ;
  gray_level_min = pt.gray_level_min ;
  gray_level_max = pt.gray_level_max ;
  grayLevelPrecision = pt.grayLevelPrecision;
  compute_moment = pt.compute_moment ;

  maxDotSizePercentage = pt.maxDotSizePercentage;

  m00 = pt.m00;
  m01 = pt.m01;
  m10 = pt.m10;
  m02 = pt.m02;
  m20 = pt.m20;

  u_min = pt.u_min;
  v_min = pt.v_min;
  u_max = pt.u_max;
  v_max = pt.v_max;

  gray_level_out = pt.gray_level_out;

  nbMaxPoint = pt.nbMaxPoint;

  return *this ;
}

bool
vpDot::operator!=(const vpDot& m)
{
  return ((cog_u!=m.cog_v) || (cog_v!=m.cog_v)) ;
}

bool
vpDot::operator==(const vpDot& m)
{
  return ((cog_u==m.cog_u) && (cog_v==m.cog_v)) ;
}

/*!

  This method choose a gray level (default is 0) used to modify the
  "in" dot level in "out" dot level. This gray level is here needed to
  stop the recursivity . The pixels of the dot are set to this new
  gray level "\out\" when connexe() is called.

  \exception vpTrackingException::initializationError : No valid gray
  level "out" can be found. This means that the min gray level is set to 0
  and the max gray level to 255. This should not occur
*/
void 
vpDot::setGrayLevelOut()
{
  if (gray_level_min == 0) {
    if (gray_level_max == 255) {
      // gray_level_min = 0 and gray_level_max = 255: this should not occur
      vpERROR_TRACE("Unable to choose a good \"out\" level") ;
      throw(vpTrackingException(vpTrackingException::initializationError,
				"Unable to choose a good \"out\" level")) ;
    }
    gray_level_out = gray_level_max + 1;
  }
}

/*!
  Perform the tracking of a dot by connex components.

  \param mean_value : Threshold to use for the next call to track()
  and corresponding to the mean value of the dot intensity.

  \warning The content of the image is modified thanks to
  setGrayLevelOut() called before. This method choose a gray level
  (default is 0) used to modify the "in" dot level in "out" dot
  level. This gray level is here needed to stop the recursivity . The
  pixels of the dot are set to this new gray level "\out\".

  \return vpDot::out if an error occurs, vpDot::in otherwise.

  \sa setGrayLevelOut()
*/
int
vpDot::connexe(vpImage<unsigned char>& I, int u, int v,
	       unsigned int gray_level_min, unsigned int gray_level_max,
	       double &mean_value, double &u_cog, double &v_cog, double &n)
{

  int width = I.getWidth();
  int height= I.getHeight();

  // Test if we are in the image
  if ( (u < 0) || (v < 0) || (u >= width) || (v >= height) ) {
    return  vpDot::out ;
  }
  if (I[v][u] >= gray_level_min && I[v][u] <= gray_level_max)
  {
    if (graphics==true)
    {
      //      printf("u %d v %d\n", u, v);
      vpDisplay::displayPoint(I,v,u,vpColor::green) ;
      //vpDisplay::flush(I);
    }
    Lu += u ;
    Lv += v ;
    u_cog += u ;
    v_cog += v ;
    n+=1 ;

    if (n > nbMaxPoint) {
      vpERROR_TRACE("Too many point %lf (%lf%% of image size). "
		    "This threshold can be modified using the setMaxDotSize() "
		    "method.",
		    n, n / (I.getWidth() * I.getHeight()),
		    nbMaxPoint, maxDotSizePercentage) ;

      throw(vpTrackingException(vpTrackingException::featureLostError,
				"Dot to big")) ;
    }

    // Bounding box update
    if (u < this->u_min) this->u_min = u;
    if (u > this->u_max) this->u_max = u;
    if (v < this->v_min) this->v_min = v;
    if (v > this->v_max) this->v_max = v;
//     if (graphics==true)
//     {
//       vpRect r(this->u_min, this->v_min,
// 	       this->u_max - this->u_min + 1,
// 	       this->v_max - this->v_min + 1);
//       vpDisplay::displayRectangle(I, r, vpColor::white) ;
//     }
    // Mean value of the dot intensities
    mean_value = (mean_value *(n-1) + I[v][u]) / n;
    if (compute_moment==true)
    {
      m00++ ;
      m10 += u ;
      m01 += v ;
      m11 += (u*v) ;
      m20 += u*u ;
      m02 += v*v ;
    }
    I[v][u] = this->gray_level_out ;
  }
  else
  {
    return vpDot::out ;
  }
  if ( u-1 >= 0)
  {
    if (I[v][u-1] >= gray_level_min && I[v][u-1] <= gray_level_max)
      connexe(I,u-1,v, gray_level_min, gray_level_max, mean_value,
	      u_cog,v_cog, n) ;
  }

  if (u+1 < width)
  {
    if (I[v][u+1] >= gray_level_min && I[v][u+1] <= gray_level_max)
      connexe(I,u+1,v,gray_level_min, gray_level_max, mean_value,
	      u_cog, v_cog, n) ;
  }
  if  (v-1 >= 0)
  {
    if (I[v-1][u] >=gray_level_min && I[v-1][u] <= gray_level_max)
      connexe(I,u, v-1,gray_level_min, gray_level_max, mean_value,
	      u_cog, v_cog, n) ;
  }
  if  (v+1 < height)
  {
    if (I[v+1][u] >=gray_level_min && I[v+1][u] <= gray_level_max)
      connexe(I,u,v+1,gray_level_min, gray_level_max, mean_value,
	      u_cog, v_cog, n) ;
  }

  if (connexity == CONNEXITY_8) {
    if ( (u-1 >= 0) && (v-1 >= 0) )
    {

      if (I[v-1][u-1] >=gray_level_min && I[v-1][u-1] <= gray_level_max)
	connexe(I,u-1,v-1,gray_level_min, gray_level_max, mean_value,
		u_cog, v_cog, n) ;
    }

    if ( (u+1 < width) && (v-1 >= 0 ) )
    {

      if (I[v-1][u+1] >=gray_level_min && I[v-1][u+1] <= gray_level_max)
	connexe(I,u+1,v-1,gray_level_min, gray_level_max, mean_value,
		u_cog, v_cog, n) ;
    }
    if  ( (v+1 < height) && (u-1 >= 0) )
    {

      if (I[v+1][u-1] >=gray_level_min && I[v+1][u-1] <= gray_level_max)
	connexe(I,u-1,v+1,gray_level_min, gray_level_max, mean_value,
		u_cog, v_cog, n) ;
    }
    if  ( (v+1 < height) && (u+1 < width) )
    {

      if (I[v+1][u+1] >=gray_level_min && I[v+1][u+1] <= gray_level_max)
	connexe(I,u+1,v+1,gray_level_min, gray_level_max, mean_value,
		u_cog, v_cog, n) ;
    }
  }

  return vpDot::in ;
}

/*!

  Compute the center of gravity (COG) of the dot using connex
  components.  We assume the origin pixel (u, v) is in the dot. If
  not, the dot is seach arround this origin using a spiral search.

  \param I : Image to process.
  \param u : Starting pixel coordinate along the columns from where the
  dot is searched .

  \param v : Starting pixel coordinate along the rows from where the
  dot is searched .

  \warning The content of the image is modified.

  \exception vpTrackingException::featureLostError : If the tracking fails.

  \sa connexe()
*/
void
vpDot::COG(vpImage<unsigned char> &I, double& u, double& v)
{
  // Set the maximal number of points considering the maximal dot size
  // image percentage
  nbMaxPoint = (I.getWidth() * I.getHeight()) *  maxDotSizePercentage;

  // segmentation de l'image apres seuillage
  // (etiquetage des composante connexe)
  if (compute_moment)
    m00 = m11 = m02 = m20 = m10 = m01 = 0 ;

  double u_cog = 0 ;
  double v_cog = 0 ;
  double npoint = 0 ;
  this->mean_gray_level = 0 ;

  Lu.kill() ;
  Lv.kill() ;

  // Initialise the boundig box
  this->u_min = I.getWidth();
  this->u_max = 0;
  this->v_min = I.getHeight();
  this->v_max = 0;

#if 0
  // Original version
  if (  connexe(I, (unsigned int)u, (unsigned int)v,
		gray_level_min, gray_level_max,
		mean_gray_level, u_cog, v_cog, npoint) == vpDot::out)
  {
    bool sol = false ;
    unsigned int pas  ;
    for (pas = 2 ; pas <= 25 ; pas ++ )if (sol==false)
    {
      for (int k=-1 ; k <=1 ; k++) if (sol==false)
	for (int l=-1 ; l <=1 ; l++) if (sol==false)
	{
	  u_cog = 0 ;
	  v_cog = 0 ;
	  Lu.kill() ;
	  Lv.kill() ;
	  this->mean_gray_level = 0 ;
	  if (connexe(I, (unsigned int)(u+k*pas),(unsigned int)(v+l*pas),
		      gray_level_min, gray_level_max,
		      mean_gray_level, u_cog, v_cog, npoint) != vpDot::out)
	  {
	    sol = true ; u += k*pas ; v += l*pas ;
	  }
	}
    }
    if (sol == false)
    {
      vpERROR_TRACE("Dot has been lost") ;
      throw(vpTrackingException(vpTrackingException::featureLostError,
				"Dot has been lost")) ;
    }
  }
#else
  // If the dot is not found, search around using a spiral
  if (  connexe(I,(unsigned int)u,(unsigned int)v,
		gray_level_min, gray_level_max,
		mean_gray_level, u_cog, v_cog, npoint) == vpDot::out)
  {

    bool sol = false ;

    unsigned int right = 1;
    unsigned int botom = 1;
    unsigned int left = 2;
    unsigned int up = 2;
    double u_ = u, v_ = v;
    unsigned int k;

    // Spiral search from the center to find the nearest dot
    while( (right < SPIRAL_SEARCH_SIZE) && (sol == false) ) {
      for (k=1; k <= right; k++) if(sol==false) {
	u_cog = 0 ;
	v_cog = 0 ;
	Lu.kill() ;
	Lv.kill() ;
	this->mean_gray_level = 0 ;
	if (connexe(I, (unsigned int)u_+k, (unsigned int)(v_),
		    gray_level_min, gray_level_max, mean_gray_level,
		    u_cog, v_cog, npoint) != vpDot::out) {
	  sol = true; u = u_+k; v = v_;
	}
      }
      u_ += k;
      right += 2;

      for (k=1; k <= botom; k++) if (sol==false) {
	u_cog = 0 ;
	v_cog = 0 ;
	Lu.kill() ;
	Lv.kill() ;
	this->mean_gray_level = 0 ;
	
	if (connexe(I, (unsigned int)(u_), (unsigned int)(v_+k),
		    gray_level_min, gray_level_max, mean_gray_level,
		    u_cog, v_cog, npoint)
	    != vpDot::out) {
	  sol = true; u = u_; v = v_+k;
	}
      }
      v_ += k;
      botom += 2;

      for (k=1; k <= left; k++) if (sol==false) {
	u_cog = 0 ;
	v_cog = 0 ;
	Lu.kill() ;
	Lv.kill() ;
	this->mean_gray_level = 0 ;

	if (connexe(I, (unsigned int)(u_-k), (unsigned int)(v_),
		    gray_level_min,  gray_level_max, mean_gray_level,
		    u_cog, v_cog, npoint)
	    != vpDot::out) {
	  sol = true ; u = u_-k; v = v_;
	}
      }
      u_ -= k;
      left += 2;

      for (k=1; k <= up; k++) if(sol==false) {
	u_cog = 0 ;
	v_cog = 0 ;
	Lu.kill() ;
	Lv.kill() ;
	this->mean_gray_level = 0 ;

	if (connexe(I, (unsigned int)(u_), (unsigned int)(v_-k),
		    gray_level_min, gray_level_max, mean_gray_level,
		    u_cog, v_cog, npoint)
	    != vpDot::out) {
	  sol = true ; u = u_; v = v_-k;
	}
      }
      v_ -= k;
      up += 2;
    }

    if (sol == false) {
      vpERROR_TRACE("Dot has been lost") ;
      throw(vpTrackingException(vpTrackingException::featureLostError,
				"Dot has been lost")) ;
    }
  }

#endif
  Lu.front() ; Lv.front() ;
  while (!Lu.outside())
  {
    unsigned int u,v ;
    u = Lu.value() ; v = Lv.value() ;
    I[v][u] = 255 ;
    Lu.next() ;
    Lv.next() ;
  }

  u_cog = u_cog/npoint ;
  v_cog = v_cog/npoint ;


  u = u_cog ;
  v = v_cog ;

  // Initialize the threshold for the next call to track()
  double Ip = pow((double)this->mean_gray_level/255,1/gamma);

  if(Ip - (1 - grayLevelPrecision)<0){
    gray_level_min = 0 ;
  }
  else{
    gray_level_min = (unsigned int) (255*pow(Ip - (1 - grayLevelPrecision),gamma));
    if (gray_level_min > 255)
      gray_level_min = 255;
  }
  gray_level_max = (unsigned int) (255*pow(Ip + (1 - grayLevelPrecision),gamma));
  if (gray_level_max > 255)
    gray_level_max = 255;

  //vpCTRACE << "gray_level_min: " << gray_level_min << std::endl;
  //vpCTRACE << "gray_level_max: " << gray_level_max << std::endl;

  if (npoint < 5)
  {
    vpERROR_TRACE("Dot to small") ;
    throw(vpTrackingException(vpTrackingException::featureLostError,
			      "Dot to small")) ;
  }

  if (npoint > nbMaxPoint)
  {
    vpERROR_TRACE("Too many point %lf (%lf%%). Max allowed is %lf (%lf%%). This threshold can be modified using the setMaxDotSize() method.",
		  npoint, npoint / (I.getWidth() * I.getHeight()),
		  nbMaxPoint, maxDotSizePercentage) ;

   throw(vpTrackingException(vpTrackingException::featureLostError,
			      "Dot to big")) ;
  }
}

/*!
  Maximal size of the region to track in terms of image size percentage.

  \param percentage : Image size percentage corresponding to the dot
  maximal size. Values should be in ]0 : 1]. If 1, that means that the
  dot to track could take up the whole image.

  During the tracking, if the dot size if bigger than the maximal size
  allowed an exception is throwed :
  vpTrackingException::featureLostError.

*/
void
vpDot::setMaxDotSize(double percentage)
{
  if (percentage <= 0.0 || percentage > 1.0) {
    // print a warning. We keep the default percentage
    vpTRACE("Max dot size percentage is requested to be set to %lf.",
	    "Value should be in ]0:1]. Value will be set to %lf.", 
	    percentage, maxDotSizePercentage);
  }
  else {
    maxDotSizePercentage = percentage;
  }
}
/*!

  Initialize the tracking with a mouse click and update the dot
  characteristics (center of gravity, moments).

  Wait a user click in a gray area in the image I. The clicked pixel
  will be the starting point from which the dot will be tracked.

  The threshold used to segment the dot is set with the grayLevelPrecision
  parameter. See the formula in setGrayLevelPrecision() function.

  The sub pixel coordinates of the dot are updated. To get the center
  of gravity coordinates of the dot, use get_u() and get_v(). To
  compute the moments use setComputeMoments(true) before a call to
  initTracking().

  \warning The content of the image modified since we call track() to
  compute the dot characteristics.

  \param I : Image to process.

  \sa track(), get_u(), get_v()
*/
void
vpDot::initTracking(vpImage<unsigned char>& I)
{
  unsigned int i1,j1;

  while (vpDisplay::getClick(I,i1,j1)!=true) ;


  double Ip = pow((double)I[i1][j1]/255,1/gamma);

  if(Ip - (1 - grayLevelPrecision)<0){
    gray_level_min = 0 ;
  }
  else{
    gray_level_min = (unsigned int) (255*pow(Ip - (1 - grayLevelPrecision),gamma));
    if (gray_level_min > 255)
      gray_level_min = 255;
  }
  gray_level_max = (unsigned int) (255*pow(Ip + (1 - grayLevelPrecision),gamma));
  if (gray_level_max > 255)
    gray_level_max = 255;

    
  double u,v ;
  u = j1 ;
  v = i1 ;

  cog_ufloat = u ;
  cog_vfloat = v ;

  if ((u-(unsigned int)u) < 0.5)
    cog_u = (unsigned int)u ;
  else
    cog_u = (unsigned int)u+1 ;

  if ((v-(unsigned int)v) < 0.5)
    cog_v = (unsigned int)v ;
  else
    cog_v = (unsigned int)v+1 ;

  try {
    track( I );
  }
  catch(...)
  {
    vpERROR_TRACE("Error caught") ;
    throw ;
  }
}

/*!

  Initialize the tracking for a dot supposed to be located at (u,v) and
  updates the dot characteristics (center of gravity, moments).

  The threshold used to segment the dot is set to 80 percent of the
  gray level of the pixel (u,v).

  The sub pixel coordinates of the dot are updated. To get the center
  of gravity coordinates of the dot, use get_u() and get_v(). To
  compute the moments use setComputeMoments(true) before a call to
  initTracking().

  \warning The content of the image modified since we call track() to
  compute the dot characteristics.

  \param I : Image to process.

  \param u : Dot location or starting point (column pixel coordinate)
  from which the dot will be tracked in the image.

  \param v : Dot location or starting point (row pixel coordinate)
  from which the dot will be tracked in the image.

  \sa track(), get_u(), get_v()
*/
void
vpDot::initTracking(vpImage<unsigned char>& I, unsigned int u, unsigned int v)
{

  cog_ufloat = u ;
  cog_vfloat = v ;

  cog_u = u ;
  cog_v = v ;

  double Ip = pow((double)I[cog_v][cog_u]/255,1/gamma);

  if(Ip - (1 - grayLevelPrecision)<0){
    gray_level_min = 0 ;
  }
  else{
    gray_level_min = (unsigned int) (255*pow(Ip - (1 - grayLevelPrecision),gamma));
    if (gray_level_min > 255)
      gray_level_min = 255;
  }
  gray_level_max = (unsigned int) (255*pow(Ip + (1 - grayLevelPrecision),gamma));
  if (gray_level_max > 255)
    gray_level_max = 255;
  try {
    track( I );
  }
  catch(...)
  {
    vpERROR_TRACE("Error caught") ;
    throw ;
  }
}

/*!

  Initialize the tracking for a dot supposed to be located at (u,v) and
  updates the dot characteristics (center of gravity, moments).


  The sub pixel coordinates of the dot are updated. To get the center
  of gravity coordinates of the dot, use get_u() and get_v(). To
  compute the moments use setComputeMoments(true) before a call to
  initTracking().

  \warning The content of the image modified since we call track() to
  compute the dot characteristics.

  \param I : Image to process.

  \param u : Dot location or starting point (column pixel coordinate)
  from which the dot will be tracked in the image.

  \param v : Dot location or starting point (row pixel coordinate)
  from which the dot will be tracked in the image.

  \param gray_level_min : Minimum gray level threshold used to segment the dot;
  value comprised between 0 and 255.

  \param gray_level_max : Maximum gray level threshold used to segment the
  dot; value comprised between 0 and 255. \e gray_level_max should be
  greater than \e gray_level_min.

  \sa track(), get_u(), get_v()
*/
void
vpDot::initTracking(vpImage<unsigned char>& I, unsigned int u, unsigned int v,
		    unsigned int gray_level_min, unsigned int gray_level_max)
{

  cog_ufloat = u ;
  cog_vfloat = v ;

  cog_u = u ;
  cog_v = v ;
  this->gray_level_min = gray_level_min;
  this->gray_level_max = gray_level_max;

  try {
    track( I );
  }
  catch(...)
  {
    vpERROR_TRACE("Error caught") ;
    throw ;
  }
}


/*!
  Track and compute the dot characteristics.

  To get the center of gravity coordinates of the dot, use get_u() and
  get_v(). To compute the moments use setComputeMoments(true) before a
  call to initTracking().

  \warning The image is modified (all the pixels that belong to the point
  are set to white (ie to 255).

  \param I : Image to process.

  \sa get_u(), get_v()
*/
void
vpDot::track(vpImage<unsigned char> &I)
{
  double u = cog_ufloat ;
  double v = cog_vfloat ;

  try{
    setGrayLevelOut();
    COG(I,u,v) ;
  }
  catch(...)
  {
    vpERROR_TRACE("Error caught") ;
    throw ;
  }

  cog_ufloat = u ;
  cog_vfloat = v ;

  if ((u-(unsigned int)u) < 0.5)
    cog_u = (unsigned int)u ;
  else  cog_u = (unsigned int)u+1 ;

  if ((v-(unsigned int)v) < 0.5)
    cog_v = (unsigned int)v ;
  else  cog_v = (unsigned int)v+1 ;
}

/*!
  Track and updates the new dot coordinates

  To compute the moments use setComputeMoments(true) before a call to
  initTracking() or track().

  \warning The image is modified (all the pixels that belong to the point
  are set to white (ie to 255).

  \param I : Image to process.

  \param u : Sub pixel coordinate (along the columns) of the tracked dot.
  \param v : Sub pixel coordinate (along the rows) of the tracked dot.
*/
void
vpDot::track(vpImage<unsigned char> &I, double &u, double &v)
{
  track(I) ;
  u = vpDot::get_u() ;
  v = vpDot::get_v() ;
}

/*!

  Set the precision of the gray level of the dot.

  \param grayLevelPrecision : It is a double precision float which value is in ]0,1]:
  - 1 means full precision, whereas values close to 0 show a very bad accuracy.
  - Values lower or equal to 0 are brought back to an epsion>0
  - Values higher than  1 are brought back to 1
  If the initial gray level is I, the gray levels of the dot will be between :
  \f$Imin=255*\big((\frac{I}{255})^{{\gamma}^{-1}}-(1-grayLevelPrecision)\big)^{\gamma}\f$
  and
  \f$Imax=255*\big((\frac{I}{255})^{{\gamma}^{-1}}+(1-grayLevelPrecision)\big)^{\gamma}\f$
  with \f$\gamma=1.5\f$ .

  \sa setWidth(), setHeight(), setSurface(), setInLevel(), setOutLevel()
*/
void vpDot::setGrayLevelPrecision( const double & grayLevelPrecision )
{
  double epsilon = 0.05;
  if( grayLevelPrecision<epsilon )
  {
    this->grayLevelPrecision = epsilon;
  }
  else if( grayLevelPrecision>1 )
  {
    this->grayLevelPrecision = 1.0;
  }
  else
  {
    this->grayLevelPrecision = grayLevelPrecision;
  }
}

/*
 * Local variables:
 * c-basic-offset: 2
 * End:
 */



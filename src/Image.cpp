/* -*- c++ -*-
 * Copyright (c) 2012-2016 by the GalSim developers team on GitHub
 * https://github.com/GalSim-developers
 *
 * This file is part of GalSim: The modular galaxy image simulation toolkit.
 * https://github.com/GalSim-developers/GalSim
 *
 * GalSim is free software: redistribution and use in source and binary forms,
 * with or without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions, and the disclaimer given in the accompanying LICENSE
 *    file.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the disclaimer given in the documentation
 *    and/or other materials provided with the distribution.
 */

//#define DEBUGLOGGING

#include <sstream>
#include <numeric>

#include "Image.h"
#include "ImageArith.h"
#include "FFT.h"

namespace galsim {

/////////////////////////////////////////////////////////////////////
//// Constructor for out-of-bounds that has coordinate info
///////////////////////////////////////////////////////////////////////


std::string MakeErrorMessage(
    const std::string& m, const int min, const int max, const int tried)
{
    // See discussion in Std.h about this initial value.
    std::ostringstream oss(" ");
    oss << "Attempt to access "<<m<<" number "<<tried
        << ", range is "<<min<<" to "<<max;
    return oss.str();
}
ImageBoundsError::ImageBoundsError(
    const std::string& m, const int min, const int max, const int tried) :
    ImageError(MakeErrorMessage(m,min,max,tried))
{}

std::string MakeErrorMessage(const int x, const int y, const Bounds<int> b)
{
    std::ostringstream oss(" ");
    bool found=false;
    if (x<b.getXMin() || x>b.getXMax()) {
        oss << "Attempt to access column number "<<x
            << ", range is "<<b.getXMin()<<" to "<<b.getXMax();
        found = true;
    }
    if (y<b.getYMin() || y>b.getYMax()) {
        if (found) oss << " and ";
        oss << "Attempt to access row number "<<y
            << ", range is "<<b.getYMin()<<" to "<<b.getYMax();
        found = true;
    }
    if (!found) return "Cannot find bounds violation ???";
    else return oss.str();
}
ImageBoundsError::ImageBoundsError(const int x, const int y, const Bounds<int> b) :
    ImageError(MakeErrorMessage(x,y,b))
{}

/////////////////////////////////////////////////////////////////////
//// Constructor (and related helpers) for the various Image classes
///////////////////////////////////////////////////////////////////////

namespace {

template <typename T>
class ArrayDeleter {
public:
    void operator()(T * p) const { delete [] p; }
};

} // anonymous

template <typename T>
BaseImage<T>::BaseImage(const Bounds<int>& b) :
    AssignableToImage<T>(b), _owner(), _data(0), _nElements(0), _step(0), _stride(0),
    _ncol(0), _nrow(0)
{
    if (this->_bounds.isDefined()) allocateMem();
    // Else _data is left as 0, step,stride = 0.
}

template <typename T>
void BaseImage<T>::allocateMem()
{
    // Note: this version always does the memory (re-)allocation.
    // So the various functions that call this should do their (different) checks
    // for whether this is necessary.
    _step = 1;
    _stride = _ncol = this->_bounds.getXMax() - this->_bounds.getXMin() + 1;
    _nrow = this->_bounds.getYMax() - this->_bounds.getYMin() + 1;

    _nElements = _stride * (this->_bounds.getYMax() - this->_bounds.getYMin() + 1);
    if (_stride <= 0 || _nElements <= 0) {
        FormatAndThrow<ImageError>() <<
            "Attempt to create an Image with defined but invalid Bounds ("<<this->_bounds<<")";
    }

    // The ArrayDeleter is because we use "new T[]" rather than an normal new.
    // Without ArrayDeleter, shared_ptr would just use a regular delete, rather
    // than the required "delete []".
    _owner.reset(new T[_nElements], ArrayDeleter<T>());
    _data = _owner.get();
}

template <typename T>
struct Sum
{
    Sum(): sum(0) {}
    void operator()(T x) { sum += x; }
    T sum;
};

template <typename T>
T BaseImage<T>::sumElements() const
{
    Sum<T> sum;
    sum = for_each_pixel(*this, sum);
    return sum.sum;
}

template <typename T>
ImageAlloc<T>::ImageAlloc(int ncol, int nrow, T init_value) :
    BaseImage<T>(Bounds<int>(1,ncol,1,nrow))
{
    if (ncol <= 0 || nrow <= 0) {
        std::ostringstream oss(" ");
        if (ncol <= 0) {
            if (nrow <= 0) {
                oss << "Attempt to create an Image with non-positive ncol ("<<
                    ncol<<") and nrow ("<<nrow<<")";
            } else {
                oss << "Attempt to create an Image with non-positive ncol ("<<
                    ncol<<")";
            }
        } else {
            oss << "Attempt to create an Image with non-positive nrow ("<<
                nrow<<")";
        }
        throw ImageError(oss.str());
    }
    fill(init_value);
}

template <typename T>
ImageAlloc<T>::ImageAlloc(const Bounds<int>& bounds, const T init_value) :
    BaseImage<T>(bounds)
{
    fill(init_value);
}

template <typename T>
void ImageAlloc<T>::resize(const Bounds<int>& new_bounds)
{
    if (!new_bounds.isDefined()) {
        // Then this is really a deallocation.  Clear out the existing memory.
        this->_bounds = new_bounds;
        this->_owner.reset();
        this->_data = 0;
        this->_nElements = 0;
        this->_step = 0;
        this->_stride = 0;
        this->_ncol = 0;
        this->_nrow = 0;
    } else if (this->_bounds.isDefined() &&
               new_bounds.area() <= this->_nElements &&
               this->_owner.unique()) {
        // Then safe to keep existing memory allocation.
        // Just redefine the bounds and stride.
        this->_bounds = new_bounds;
        this->_stride = this->_ncol = new_bounds.getXMax() - new_bounds.getXMin() + 1;
        this->_nrow = new_bounds.getYMax() - new_bounds.getYMin() + 1;
    } else {
        // Then we want to do the reallocation.
        this->_bounds = new_bounds;
        this->allocateMem();
    }
}


/////////////////////////////////////////////////////////////////////
//// Access methods
///////////////////////////////////////////////////////////////////////

template <typename T>
const T& BaseImage<T>::at(const int xpos, const int ypos) const
{
    if (!_data) throw ImageError("Attempt to access values of an undefined image");
    if (!this->_bounds.includes(xpos, ypos)) throw ImageBoundsError(xpos, ypos, this->_bounds);
    return _data[addressPixel(xpos, ypos)];
}

template <typename T>
T& ImageView<T>::at(const int xpos, const int ypos)
{
    if (!this->_data) throw ImageError("Attempt to access values of an undefined image");
    if (!this->_bounds.includes(xpos, ypos)) throw ImageBoundsError(xpos, ypos, this->_bounds);
    return this->_data[this->addressPixel(xpos, ypos)];
}

template <typename T>
T& ImageAlloc<T>::at(const int xpos, const int ypos)
{
    if (!this->_data) throw ImageError("Attempt to access values of an undefined image");
    if (!this->_bounds.includes(xpos, ypos)) throw ImageBoundsError(xpos, ypos, this->_bounds);
    return this->_data[this->addressPixel(xpos, ypos)];
}

template <typename T>
const T& ImageAlloc<T>::at(const int xpos, const int ypos) const
{
    if (!this->_data) throw ImageError("Attempt to access values of an undefined image");
    if (!this->_bounds.includes(xpos, ypos)) throw ImageBoundsError(xpos, ypos, this->_bounds);
    return this->_data[this->addressPixel(xpos, ypos)];
}

template <typename T>
ConstImageView<T> BaseImage<T>::subImage(const Bounds<int>& bounds) const
{
    if (!_data) throw ImageError("Attempt to make subImage of an undefined image");
    if (!this->_bounds.includes(bounds)) {
        FormatAndThrow<ImageError>() <<
            "Subimage bounds (" << bounds << ") are outside original image bounds (" <<
            this->_bounds << ")";
    }
    T* newdata = _data
        + (bounds.getYMin() - this->_bounds.getYMin()) * _stride
        + (bounds.getXMin() - this->_bounds.getXMin()) * _step;
    return ConstImageView<T>(newdata,_owner,_step,_stride,bounds);
}

template <typename T>
ImageView<T> ImageView<T>::subImage(const Bounds<int>& bounds)
{
    if (!this->_data) throw ImageError("Attempt to make subImage of an undefined image");
    if (!this->_bounds.includes(bounds)) {
        FormatAndThrow<ImageError>() <<
            "Subimage bounds (" << bounds << ") are outside original image bounds (" <<
            this->_bounds << ")";
    }
    T* newdata = this->_data
        + (bounds.getYMin() - this->_bounds.getYMin()) * this->_stride
        + (bounds.getXMin() - this->_bounds.getXMin()) * this->_step;
    return ImageView<T>(newdata,this->_owner,this->_step,this->_stride,bounds);
}

template <typename T>
ImageView<T> ImageView<T>::wrap(const Bounds<int>& b)
{
    dbg<<"Start ImageView::wrap: b = "<<b<<std::endl;

    const int i1 = b.getXMin()-this->_bounds.getXMin();
    const int i2 = b.getXMax()-this->_bounds.getXMin()+1;  // +1 for "1 past the end"
    const int j1 = b.getYMin()-this->_bounds.getYMin();
    const int j2 = b.getYMax()-this->_bounds.getYMin()+1;
    xdbg<<"i1,i2,j1,j2 = "<<i1<<','<<i2<<','<<j1<<','<<j2<<std::endl;
    const int mwrap = i2-i1;
    const int nwrap = j2-j1;
    const int skip = this->getNSkip();
    const int step = this->getStep();
    const int stride = this->getStride();
    const int m = this->getNCol();
    const int n = this->getNRow();
    T* ptr = this->getData();

    // First wrap the rows into the range [j1,j2).
    // Row 0 maps onto j2 - (j2 % nwrap)
    int jj = j2 - (j2 % nwrap);
    T* ptrwrap = ptr + jj * stride;
    for (int j=0; j<n; ++j, ++jj, ptr+=skip, ptrwrap+=skip) {
        // Loop jj and ptrwrap back if necessary.
        if (jj == j2) {
            jj = j1;
            ptrwrap -= nwrap * stride;
        }
        // When we get here, we can just skip to j2 and keep going.
        if (j == j1) {
            assert(jj == j1);
            assert(ptr == ptrwrap);
            j = j2;
            ptr += nwrap * stride;
            if (j2 == n) break;
        }
        xdbg<<"Wrap row "<<j<<" onto row = "<<jj<<std::endl;
        xdbg<<"ptrs = "<<ptr-this->getData()<<"  "<<ptrwrap-this->getData()<<std::endl;
        // Add contents of row j to row jj
        if (step == 1)
            for(int i=0; i<m; ++i) *ptrwrap++ += *ptr++;
        else
            for(int i=0; i<m; ++i,ptr+=step,ptrwrap+=step) *ptrwrap += *ptr;
    }

    // Next wrap rows [j1,j2) into the columns [i1,i2).
    ptr = getData() + j1*stride;
    for (int j=j1; j<j2; ++j, ptr+=skip) {
        int ii = i2 - (i2 % mwrap);
        ptrwrap = ptr + ii*step;
        xdbg<<"Wrap row "<<j<<" into columns ["<<i1<<','<<i2<<")\n";
        xdbg<<"ptrs = "<<ptr-this->getData()<<"  "<<ptrwrap-this->getData()<<std::endl;
        for (int i=0; i<m; ++i, ++ii, ptr+=step, ptrwrap+=step) {
            // Loop ii and ptrwrap back if necessary.
            if (ii == i2) {
                ii = i1;
                ptrwrap -= mwrap * step;
            }
            // When we get here, we can just skip to i2 and keep going.
            if (i == i1) {
                assert(ii == i1);
                assert(ptr == ptrwrap);
                i = i2;
                ptr += mwrap * step;
                if (i2 == m) break;
            }
            xxdbg<<i<<" "<<ii<<" "<<ptr-this->getData()<<" "<<ptrwrap-this->getData()<<std::endl;
            // Add contents of pixel i to pixel ii
            *ptrwrap += *ptr;
        }
    }

    return subImage(b);
}

namespace {

template <typename T>
class ConstReturn
{
public:
    ConstReturn(const T v): val(v) {}
    T operator()(const T ) const { return val; }
private:
    T val;
};

template <typename T>
class ReturnInverse
{
public:
    T operator()(const T val) const { return val==T(0) ? T(0.) : T(1./val); }
};

template <typename T>
class ReturnSecond
{
public:
    T operator()(T, T v) const { return v; }
};

} // anonymous

template <typename T>
void ImageView<T>::fill(T x)
{
    transform_pixel(*this, ConstReturn<T>(x));
}

template <typename T>
void ImageView<T>::invertSelf()
{
    transform_pixel(*this, ReturnInverse<T>());
}

template <typename T>
void ImageView<T>::copyFrom(const BaseImage<T>& rhs)
{
    if (!this->_bounds.isSameShapeAs(rhs.getBounds()))
        throw ImageError("Attempt im1 = im2, but bounds not the same shape");
    transform_pixel(*this, rhs, ReturnSecond<T>());
}

// instantiate for expected types

template class BaseImage<double>;
template class BaseImage<float>;
template class BaseImage<int32_t>;
template class BaseImage<int16_t>;
template class BaseImage<std::complex<double> >;
template class ImageAlloc<double>;
template class ImageAlloc<float>;
template class ImageAlloc<int32_t>;
template class ImageAlloc<int16_t>;
template class ImageAlloc<std::complex<double> >;
template class ImageView<double>;
template class ImageView<float>;
template class ImageView<int32_t>;
template class ImageView<int16_t>;
template class ImageView<std::complex<double> >;
template class ConstImageView<double>;
template class ConstImageView<float>;
template class ConstImageView<int32_t>;
template class ConstImageView<int16_t>;
template class ConstImageView<std::complex<double> >;

} // namespace galsim


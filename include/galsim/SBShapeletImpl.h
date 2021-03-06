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

#ifndef GalSim_SBShapeletImpl_H
#define GalSim_SBShapeletImpl_H

#include "SBProfileImpl.h"
#include "SBShapelet.h"

namespace galsim {

    class SBShapelet::SBShapeletImpl : public SBProfile::SBProfileImpl
    {
    public:
        SBShapeletImpl(double sigma, const LVector& bvec, const GSParamsPtr& gsparams);
        ~SBShapeletImpl() {}

        double xValue(const Position<double>& p) const;
        std::complex<double> kValue(const Position<double>& k) const;


        double maxK() const;
        double stepK() const;

        bool isAxisymmetric() const { return false; }
        bool hasHardEdges() const { return false; }
        bool isAnalyticX() const { return true; }
        bool isAnalyticK() const { return true; }

        Position<double> centroid() const;

        double getFlux() const;
        double getSigma() const;
        const LVector& getBVec() const;

        /// @brief Photon-shooting is not implemented for SBShapelet, will throw an exception.
        boost::shared_ptr<PhotonArray> shoot(int N, UniformDeviate ud) const
        { throw SBError("SBShapelet::shoot() is not implemented"); }

        // Overrides for better efficiency
        void fillXValue(tmv::MatrixView<double> val,
                        double x0, double dx, int izero,
                        double y0, double dy, int jzero) const;
        void fillXValue(tmv::MatrixView<double> val,
                        double x0, double dx, double dxy,
                        double y0, double dy, double dyx) const;
        void fillKValue(tmv::MatrixView<std::complex<double> > val,
                        double kx0, double dkx, int izero,
                        double ky0, double dky, int jzero) const;
        void fillKValue(tmv::MatrixView<std::complex<double> > val,
                        double kx0, double dkx, double dkxy,
                        double ky0, double dky, double dkyx) const;

        // The above functions just build a list of (x,y) values and then call these:
        void fillXValue(tmv::MatrixView<double> val,
                        const tmv::Matrix<double>& x,
                        const tmv::Matrix<double>& y) const;
        void fillKValue(tmv::MatrixView<std::complex<double> > val,
                        const tmv::Matrix<double>& kx,
                        const tmv::Matrix<double>& ky) const;

        std::string serialize() const;

    private:
        double _sigma;
        LVector _bvec;

        // Copy constructor and op= are undefined.
        SBShapeletImpl(const SBShapeletImpl& rhs);
        void operator=(const SBShapeletImpl& rhs);
    };

}

#endif

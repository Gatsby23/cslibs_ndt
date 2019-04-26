#pragma once

#include <cslibs_ndt/matching/match_traits.hpp>
#include <cslibs_ndt/matching/parameter.hpp>
#include <cslibs_ndt_2d/static_maps/gridmap.hpp>
#include <cslibs_ndt_2d/dynamic_maps/gridmap.hpp>
#include <cslibs_ndt_2d/matching/jacobian.hpp>
#include <cslibs_ndt_2d/matching/hessian.hpp>

namespace cslibs_ndt {
namespace matching {

template<typename MapT> struct Is2dGridmap : std::false_type {};
template<> struct Is2dGridmap<cslibs_ndt_2d::dynamic_maps::Gridmap<double>> : std::true_type {};
template<> struct Is2dGridmap<cslibs_ndt_2d::static_maps::Gridmap<double>> : std::true_type {};

template<typename MapT>
struct MatchTraits<MapT, typename std::enable_if<Is2dGridmap<MapT>::value>::type>
{
    static constexpr int LINEAR_DIMS  = 2;
    static constexpr int ANGULAR_DIMS = 1;
    using Jacobian              = cslibs_ndt_2d::matching::Jacobian;
    using Hessian               = cslibs_ndt_2d::matching::Hessian;

    using gradient_t            = Eigen::Matrix<double, 3, 1>;
    using hessian_t             = Eigen::Matrix<double, 3, 3>;
    using angular_t  = Eigen::Matrix<double, 1, 1>;

    using point_t               = cslibs_math_2d::Point2d;
    using transform_t           = cslibs_math_2d::Transform2d;
    using parameter_t           = cslibs_ndt::matching::Parameter;
    using distribution_bundle_t = typename MapT::distribution_bundle_t;
    using index_t               = typename MapT::index_t;

    static transform_t makeTransform(const Eigen::Vector2d& linear,
                                     const angular_t& angular)
    {
        return transform_t{linear.x(), linear.y(), angular.value()};
    }

    static void computeGradient(const MapT& map,
                                const point_t& point,
                                const Jacobian& J,
                                const Hessian& H,
                                const parameter_t& param,
                                double& score,
                                gradient_t& g,
                                hessian_t& h)
    {
        auto* bundle = map.getDistributionBundle(point);
        if (!bundle)
            return;

        for (auto* distribution_wrapper : *bundle)
        {
            auto& d = distribution_wrapper->data();
            if (d.getN() < 3)
                continue;

            const auto info   = d.getInformationMatrix();
            const auto q      = (point.data() - d.getMean()).eval();
            const auto q_info = (q.transpose() * info).eval();
            const auto e      = -0.5 * double(q_info * q);
            const auto s      = std::exp(e);
            if (!std::isnormal(s) || s <= 1e-5)
                continue;

            // this part should be vectorized, may also remove common factors...
            for (std::size_t i = 0; i < LINEAR_DIMS + ANGULAR_DIMS; ++i)
            {
                const auto J_iq     = J.get(i, q);
                const auto J_info   = (info * J_iq).eval();

                g(i) += s * q_info * J_iq;

                for (std::size_t j = 0; j < LINEAR_DIMS + ANGULAR_DIMS; ++j)
                {
                    h(i, j) -= s * q_info * H.get(i, j, q) +
                            s * static_cast<double>((J.get(j, q).transpose()).eval() * J_info) -
                            s * (q_info * J_iq).eval() * (-q_info * J.get(j, q)).eval();
                }
            }
            std::cout << "hessian  : " << h << "\n";
            std::cout << "sub score: " << s << "\n";

            score += s;
        }
    }

    static void computeGradientComplete(const MapT& map,
                                        const distribution_bundle_t& bundle,
                                        const Jacobian& J,
                                        const Hessian& H,
                                        const transform_t &t,
                                        double& score,
                                        gradient_t& g,
                                        hessian_t& h)
    {
        auto process_bundle = [&bundle, &J, &H, &t, &score, &g, &h](const index_t &, const distribution_bundle_t &b)
        {
            computeGradient(b.data(), bundle.data(), J, H, t, score, g, h);
        };
        map.traverse(process_bundle);
    }

    static void computeGradientLayerByLayer(const MapT& map,
                                            const distribution_bundle_t& bundle,
                                            const Jacobian& J,
                                            const Hessian& H,
                                            const transform_t &t,
                                            double& score,
                                            gradient_t& g,
                                            hessian_t& h)
    {
        /// I.      : get the different distributions from the layers
        const std::size_t size = distribution_bundle_t::size();
        typename distribution_bundle_t::data_t bundle_map;

        for(std::size_t i = 0 ; i < size ; ++i) {
            const auto &d = bundle[i]->data();
            auto* bm = map.getDistributionBundle(cslibs_math_2d::Point2d(d.getMean()));
            if(!bm) {
                bundle_map[i] = nullptr;
            } else {
                bundle_map[i] = (*bm)[i];
            }
        }
        computeGradient(bundle_map, bundle.data(),
                        J, H, t,
                        score, g, h);
    }

    static void computeGradient(const MapT& map,
                                const distribution_bundle_t& bundle,
                                const Jacobian& J,
                                const Hessian& H,
                                const transform_t &t,
                                double& score,
                                gradient_t& g,
                                hessian_t& h)
    {
        /// I.      : get mean of dstributions
        Eigen::Vector2d mean = Eigen::Vector2d::Zero();
        std::size_t     valid = 0;
        for(const auto &dw : bundle) {
            const auto &d = dw->data();
            if(d.valid()) {
                mean += d.getMean();
                ++valid;
            }
        }
        mean /= static_cast<double>(valid);

        /// II.     : get a bundle from the map

        auto* bundle_map = map.getDistributionBundle(cslibs_math_2d::Point2d(mean));
        if (!bundle_map)
            return;

        computeGradient(bundle_map->data(), bundle.data(),
                        J, H, t,
                        score, g, h);
    }

    static void computeGradient(const typename distribution_bundle_t::data_t& bundle_map,
                                const typename distribution_bundle_t::data_t& bundle,
                                const Jacobian& J,
                                const Hessian& H,
                                const transform_t &t,
                                double& score,
                                gradient_t& g,
                                hessian_t& h)
    {
        const std::size_t size = distribution_bundle_t::size();

        /// III.    : calculate the score using both bundles
        const auto R = J.rotation();
        for(std::size_t i = 0 ; i < size ; ++i) {
            if(!bundle_map[i])
                continue;

            auto& d = bundle[i]->data();
            auto& d_map = bundle_map[i]->data();

            if (!d.valid() || !d_map.valid())
                continue;

            const auto mean      = t * d.getMean();
            const auto mean_map  = d_map.getMean();
            const auto cov       = d.getCovariance();
            const auto cov_map   = d_map.getCovariance();
            const auto cov_rot   = (R.transpose() * cov * R).eval();
            const auto B_inv     = (cov_rot + cov_map);

            if(B_inv.determinant() == 0.0)
                continue;

            const auto B = B_inv.inverse();
            const auto q = (mean - mean_map).eval();
            const auto q_info = (q.transpose() * B).eval();
            const auto e = static_cast<double>(q_info * q);
            const auto d2 = 1.0; // 0.05;
            const auto s = std::exp(-0.5 * d2 * e);
            const auto Bq = (B * q).eval();

            if (!std::isnormal(s) || s <= 1e-5)
                continue;

            // this part should be vectorized, may also remove common factors...
            for (std::size_t i = 0; i < LINEAR_DIMS + ANGULAR_DIMS; ++i)
            {
                const auto J_iq       = J.get(i, q);
                const auto Z_i        = J.get(i, cov_rot);
                const auto q_info_Z_i = q_info * Z_i;

                g(i) += s * d2 * 0.5 * ((q_info * J_iq).value() - (q_info_Z_i * Bq).value());

                for (std::size_t j = 0; j < LINEAR_DIMS + ANGULAR_DIMS; ++j)
                {
                    const auto H_ij = H.get(i,j,q);
                    const auto Z_ij = H.get(i,j,cov_rot);
                    const auto Z_j  = J.get(j,cov_rot);

                    h(i, j) -= s * d2 * ((J_iq.transpose() * B * J_iq).value() -
                                         (2.0 * q_info_Z_i * J_iq).value() +
                                         (q_info * H_ij).value() -
                                         (q_info_Z_i * B * Z_j * Bq).value() -
                                         (0.5 * q_info * Z_ij * Bq).value() -
                                         (d2 * 0.25 * e * e));
                }
            }

            std::cout << "hessian  : " << h << "\n";
            std::cout << "sub score: " << s << "\n";

            score += s;
        }
    }
};

}
}

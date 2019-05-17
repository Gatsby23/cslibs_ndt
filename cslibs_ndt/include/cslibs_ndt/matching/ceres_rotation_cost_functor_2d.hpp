#ifndef CSLIBS_NDT_MATCHING_CERES_ROTATION_COST_FUNCTOR_2D_HPP
#define CSLIBS_NDT_MATCHING_CERES_ROTATION_COST_FUNCTOR_2D_HPP

#include <ceres/cost_function.h>
#include <ceres/autodiff_cost_function.h>
#include <ceres/jet.h>

namespace cslibs_ndt {
namespace matching {

class CeresRotationCostFunctor2d
{
public:
    static ceres::CostFunction* CreateAutoDiffCostFunction(double weight,
                                                           const double& rotation)
    {
        return new ceres::AutoDiffCostFunction<CeresRotationCostFunctor2d, 1, 1>(
                new CeresRotationCostFunctor2d(weight, rotation)
        );
    }

    template<typename T>
    bool operator()(const T* const rotation, T* residual) const
    {
        T delta;
        T r0(rotation_);
        AngleDifference(&r0, rotation, &delta);

        residual[0] = weight_ * delta;//[0];
        //residual[1] = weight_ * delta[1];
        //residual[2] = weight_ * delta[2];
        return true;
    }

private:
    template<typename T>
    static void AngleDifference(const T* const r0, const T* const r, T* const d)
    {
        static const double _2_M_PI = 2.0 * M_PI;
        auto norm = [](const T* const r) {
            return ceres::atan2(ceres::sin(r[0]), ceres::cos(r[0]));
        };

        const auto a = norm(r0);
        const auto b = norm(r);

        const auto d1 = a - b;
        const auto d2 = (_2_M_PI - ceres::abs(d1)) * (d1 > T(0) ? T(-1) : T(1));
        *d = ceres::abs(d1) < ceres::abs(d2) ? d1 : d2;
    }

    explicit CeresRotationCostFunctor2d(double weight,
                                        const double& rotation) :
        weight_(weight),
        rotation_(rotation)
    {
    }

private:
    const double weight_;
    const double rotation_;
};

}
}

#endif // CSLIBS_NDT_MATCHING_CERES_ROTATION_COST_FUNCTOR_2D_HPP

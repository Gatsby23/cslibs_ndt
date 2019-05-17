#ifndef CSLIBS_NDT_MATCHING_CERES_TRANSLATION_COST_FUNCTOR_3D_HPP
#define CSLIBS_NDT_MATCHING_CERES_TRANSLATION_COST_FUNCTOR_3D_HPP

#include <cslibs_math_3d/linear/vector.hpp>

#include <ceres/cost_function.h>
#include <ceres/autodiff_cost_function.h>
#include <ceres/jet.h>

namespace cslibs_ndt {
namespace matching {

class CeresTranslationCostFunctor3d
{
public:
    static ceres::CostFunction* CreateAutoDiffCostFunction(double weight,
                                                           const cslibs_math_3d::Vector3d& translation)
    {
        return new ceres::AutoDiffCostFunction<CeresTranslationCostFunctor3d, 3, 3>(
                new CeresTranslationCostFunctor3d(weight, translation)
        );
    }

    template<typename T>
    bool operator()(const T* const translation, T* residual) const
    {
        for (auto i = 0; i < 3; ++i)
            residual[i] = weight_ * (translation[i] - translation_[i]);

        return true;
    }

private:
    explicit CeresTranslationCostFunctor3d(double weight,
                                           const cslibs_math_3d::Vector3d& translation) :
        weight_(weight),
        translation_({
                translation(0),
                translation(1),
                translation(2)})
    {
    }

private:
    const double weight_;
    const std::array<double, 3> translation_;
};

}
}

#endif // CSLIBS_NDT_MATCHING_CERES_TRANSLATION_COST_FUNCTOR_3D_HPP

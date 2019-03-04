#ifndef CSLIBS_NDT_2D_DYNAMIC_MAPS_OCCUPANCY_GRIDMAP_HPP
#define CSLIBS_NDT_2D_DYNAMIC_MAPS_OCCUPANCY_GRIDMAP_HPP

#include <array>
#include <vector>
#include <cmath>
#include <memory>

#include <cslibs_math_2d/linear/pose.hpp>
#include <cslibs_math_2d/linear/point.hpp>

#include <cslibs_ndt/common/occupancy_distribution.hpp>
#include <cslibs_ndt/common/bundle.hpp>

#include <cslibs_math/linear/pointcloud.hpp>
#include <cslibs_math/common/array.hpp>
#include <cslibs_math/common/div.hpp>
#include <cslibs_math/common/mod.hpp>

#include <cslibs_indexed_storage/storage.hpp>
#include <cslibs_indexed_storage/backend/kdtree/kdtree.hpp>
#include <cslibs_indexed_storage/operations/clustering/grid_neighborhood.hpp>

#include <cslibs_gridmaps/utility/inverse_model.hpp>

#include <cslibs_math_2d/algorithms/bresenham.hpp>
#include <cslibs_math_2d/algorithms/simple_iterator.hpp>
#include <cslibs_math_2d/algorithms/efla_iterator.hpp>

namespace cis = cslibs_indexed_storage;

namespace cslibs_ndt_2d {
namespace dynamic_maps {
template <typename T>
class EIGEN_ALIGN16 OccupancyGridmap
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    using allocator_t = Eigen::aligned_allocator<OccupancyGridmap>;

    using ConstPtr                          = std::shared_ptr<const OccupancyGridmap<T>>;
    using Ptr                               = std::shared_ptr<OccupancyGridmap<T>>;
    using pose_t                            = cslibs_math_2d::Pose2d<T>;
    using transform_t                       = cslibs_math_2d::Transform2d<T>;
    using point_t                           = cslibs_math_2d::Point2d<T>;
    using index_t                           = std::array<int, 2>;
    using distribution_t                    = cslibs_ndt::OccupancyDistribution<T,2>;
    using distribution_storage_t            = cis::Storage<distribution_t, index_t, cis::backend::kdtree::KDTree>;
    using distribution_storage_ptr_t        = std::shared_ptr<distribution_storage_t>;
    using distribution_storage_array_t      = std::array<distribution_storage_ptr_t, 4>;
    using distribution_bundle_t             = cslibs_ndt::Bundle<distribution_t*, 4>;
    using distribution_const_bundle_t       = cslibs_ndt::Bundle<const distribution_t*, 4>;
    using distribution_bundle_storage_t     = cis::Storage<distribution_bundle_t, index_t, cis::backend::kdtree::KDTree>;
    using distribution_bundle_storage_ptr_t = std::shared_ptr<distribution_bundle_storage_t>;
    using simple_iterator_t                 = cslibs_math_2d::algorithms::SimpleIterator<T>;
    using inverse_sensor_model_t            = cslibs_gridmaps::utility::InverseModel<T>;

    inline OccupancyGridmap(const pose_t &origin,
                            const T      &resolution) :
        resolution_(resolution),
        bundle_resolution_(0.5 * resolution_),
        bundle_resolution_inv_(1.0 / bundle_resolution_),
        w_T_m_(origin),
        m_T_w_(w_T_m_.inverse()),
        min_index_{{std::numeric_limits<int>::max(), std::numeric_limits<int>::max()}},
        max_index_{{std::numeric_limits<int>::min(), std::numeric_limits<int>::min()}},
        storage_{{distribution_storage_ptr_t(new distribution_storage_t),
                 distribution_storage_ptr_t(new distribution_storage_t),
                 distribution_storage_ptr_t(new distribution_storage_t),
                 distribution_storage_ptr_t(new distribution_storage_t)}},
        bundle_storage_(new distribution_bundle_storage_t)
    {
    }

    inline OccupancyGridmap(const pose_t  &origin,
                            const T       &resolution,
                            const index_t &min_index,
                            const index_t &max_index,
                            const std::shared_ptr<distribution_bundle_storage_t> &bundles,
                            const distribution_storage_array_t                   &storage) :
        resolution_(resolution),
        bundle_resolution_(0.5 * resolution_),
        bundle_resolution_inv_(1.0 / bundle_resolution_),
        w_T_m_(origin),
        m_T_w_(w_T_m_.inverse()),
        min_index_(min_index),
        max_index_(max_index),
        storage_(storage),
        bundle_storage_(bundles)
    {
    }

    inline OccupancyGridmap(const T &origin_x,
                            const T &origin_y,
                            const T &origin_phi,
                            const T &resolution) :
        resolution_(resolution),
        bundle_resolution_(0.5 * resolution_),
        bundle_resolution_inv_(1.0 / bundle_resolution_),
        w_T_m_(origin_x, origin_y, origin_phi),
        m_T_w_(w_T_m_.inverse()),
        min_index_{{std::numeric_limits<int>::max(), std::numeric_limits<int>::max()}},
        max_index_{{std::numeric_limits<int>::min(), std::numeric_limits<int>::min()}},
        storage_{{distribution_storage_ptr_t(new distribution_storage_t),
                 distribution_storage_ptr_t(new distribution_storage_t),
                 distribution_storage_ptr_t(new distribution_storage_t),
                 distribution_storage_ptr_t(new distribution_storage_t)}},
        bundle_storage_(new distribution_bundle_storage_t)
    {
    }

    inline OccupancyGridmap(const OccupancyGridmap &other) :
        resolution_(other.resolution_),
        bundle_resolution_(other.bundle_resolution_),
        bundle_resolution_inv_(other.bundle_resolution_inv_),
        w_T_m_(other.w_T_m_),
        m_T_w_(other.m_T_w_),
        min_index_(other.min_index_),
        max_index_(other.max_index_),
        storage_{{distribution_storage_ptr_t(new distribution_storage_t(*other.storage_[0])),
        distribution_storage_ptr_t(new distribution_storage_t(*other.storage_[1])),
        distribution_storage_ptr_t(new distribution_storage_t(*other.storage_[2])),
        distribution_storage_ptr_t(new distribution_storage_t(*other.storage_[3]))}},
        bundle_storage_(new distribution_bundle_storage_t(*other.bundle_storage_))
    {
    }

    inline OccupancyGridmap(OccupancyGridmap &&other) :
        resolution_(other.resolution_),
        bundle_resolution_(other.bundle_resolution_),
        bundle_resolution_inv_(other.bundle_resolution_inv_),
        w_T_m_(std::move(other.w_T_m_)),
        m_T_w_(std::move(other.m_T_w_)),
        min_index_(other.min_index_),
        max_index_(other.max_index_),
        storage_(other.storage_),
        bundle_storage_(other.bundle_storage_)
    {
    }

    inline bool empty() const
    {
        return min_index_[0] == std::numeric_limits<int>::max();
    }

    /**
     * @brief Get minimum in map coordinates.
     * @return the minimum
     */
    inline point_t getMin() const
    {
        return point_t(min_index_[0] * bundle_resolution_,
                       min_index_[1] * bundle_resolution_);
    }

    /**
     * @brief Get maximum in map coordinates.
     * @return the maximum
     */
    inline point_t getMax() const
    {
        return point_t((max_index_[0] + 1) * bundle_resolution_,
                       (max_index_[1] + 1) * bundle_resolution_);
    }

    /**
     * @brief Get the map origin
     * @return the origin
     */
    inline pose_t getOrigin() const
    {
        pose_t origin = w_T_m_;
        origin.translation() += point_t(min_index_[0] * bundle_resolution_,
                                        min_index_[1] * bundle_resolution_);
        return origin;
    }

    /**
     * @brief Get the initial origin of the map.
     * @return the inital origin
     */
    inline pose_t getInitialOrigin() const
    {
        return w_T_m_;
    }

    template <typename line_iterator_t = simple_iterator_t>
    inline void insert(const point_t &start_p,
                       const point_t &end_p)
    {
        const index_t &end_index = toBundleIndex(end_p);
        updateOccupied(end_index, end_p);

        line_iterator_t it(m_T_w_ * start_p, m_T_w_ * end_p, bundle_resolution_);
        while (!it.done()) {
            updateFree({{it.x(), it.y()}});
            ++ it;
        }
    }

    template <typename line_iterator_t = simple_iterator_t>
    inline void insert(const typename cslibs_math::linear::Pointcloud<point_t>::ConstPtr &points,
                       const pose_t &points_origin = pose_t())
    {
        distribution_storage_t storage;
        for (const auto &p : *points) {
            const point_t pm = points_origin * p;
            if (pm.isNormal()) {
                const index_t &bi = toBundleIndex(pm);
                distribution_t *d = storage.get(bi);
                (d ? d : &storage.insert(bi, distribution_t()))->updateOccupied(pm);
            }
        }

        const point_t start_p = m_T_w_ * points_origin.translation();
        storage.traverse([this, &start_p](const index_t& bi, const distribution_t &d) {
            if (!d.getDistribution())
                return;
            updateOccupied(bi, d.getDistribution());

            line_iterator_t it(start_p, m_T_w_ * point_t(d.getDistribution()->getMean()), bundle_resolution_);
            const std::size_t n = d.numOccupied();
            while (!it.done()) {
                updateFree({{it.x(), it.y()}}, n);
                ++ it;
            }
        });
    }

    template <typename line_iterator_t = simple_iterator_t>
    inline void insertVisible(const pose_t &origin,
                              const typename cslibs_math::linear::Pointcloud<point_t>::ConstPtr &points,
                              const typename inverse_sensor_model_t::Ptr &ivm,
                              const typename inverse_sensor_model_t::Ptr &ivm_visibility)
    {
        if (!ivm || !ivm_visibility) {
            std::cout << "[OccupancyGridmap2D]: Cannot evaluate visibility, using model-free update rule instead!" << std::endl;
            return insert(points, origin);
        }

        const index_t start_bi = toBundleIndex(origin.translation());
        auto occupancy = [this, &ivm](const index_t &bi) {
            distribution_bundle_t *bundle = getAllocate(bi);
            return 0.25 * (bundle->at(0)->getOccupancy(ivm) +
                           bundle->at(1)->getOccupancy(ivm) +
                           bundle->at(2)->getOccupancy(ivm) +
                           bundle->at(3)->getOccupancy(ivm));
        };
        auto current_visibility = [this, &start_bi, &ivm_visibility, &occupancy](const index_t &bi) {
            const T occlusion_prob =
                    std::min(occupancy({{bi[0] + ((bi[0] > start_bi[0]) ? -1 : 1), bi[1]}}),
                             occupancy({{bi[0], bi[1] + ((bi[1] > start_bi[1]) ? -1 : 1)}}));
            return ivm_visibility->getProbFree() * occlusion_prob +
                    ivm_visibility->getProbOccupied() * (1.0 - occlusion_prob);
        };

        distribution_storage_t storage;
        for (const auto &p : *points) {
            const point_t pm = origin * p;
            if (pm.isNormal()) {
                const index_t &bi = toBundleIndex(pm);
                distribution_t *d = storage.get(bi);
                (d ? d : &storage.insert(bi, distribution_t()))->updateOccupied(pm);
            }
        }

        const point_t start_p = m_T_w_ * origin.translation();
        storage.traverse([this, &ivm_visibility, &start_p, &current_visibility](const index_t& bi, const distribution_t &d) {
            if (!d.getDistribution())
                return;

            const point_t end_p = m_T_w_ * point_t(d.getDistribution()->getMean());
            line_iterator_t it(start_p, end_p, bundle_resolution_);

            const std::size_t n = d.numOccupied();
            T visibility = 1.0;
            while (!it.done()) {
                const index_t bit = {{it.x(), it.y()}};
                if ((visibility *= current_visibility(bit)) < ivm_visibility->getProbPrior())
                    return;

                updateFree(bit, n);
                ++ it;
            }

            if ((visibility *= current_visibility(bi)) >= ivm_visibility->getProbPrior())
                updateOccupied(bi, d.getDistribution());
        });
    }

    template <typename line_iterator_t = simple_iterator_t>
    inline T getRange(const point_t &start_p,
                      const point_t &end_p,
                      const typename inverse_sensor_model_t::Ptr &ivm,
                      const T &occupied_threshold) const
    {
        if (!ivm)
            throw std::runtime_error("[OccupancyGridMap]: inverse model not set");

        const index_t start_index = {{static_cast<int>(std::floor(start_p(0) * bundle_resolution_inv_)),
                                      static_cast<int>(std::floor(start_p(1) * bundle_resolution_inv_))}};
        const index_t end_index   = {{static_cast<int>(std::floor(end_p(0) * bundle_resolution_inv_)),
                                      static_cast<int>(std::floor(end_p(1) * bundle_resolution_inv_))}};
        line_iterator_t it(start_index, end_index);

        auto occupied = [this, &ivm, &occupied_threshold](const index_t &bi) {
            distribution_bundle_t *bundle = bundle_storage_->get(bi);

            return bundle && (0.25 * ((bundle->at(0)->getOccupancy(ivm)) +
                                      (bundle->at(1)->getOccupancy(ivm)) +
                                      (bundle->at(2)->getOccupancy(ivm)) +
                                      (bundle->at(3)->getOccupancy(ivm))) >= occupied_threshold);
        };

        while (!it.done()) {
            if (occupied({{it.x(), it.y()}}))
                return (start_p - point_t(it.x() * bundle_resolution_, it.y() * bundle_resolution_)).length();

            ++ it;
        }

        return (start_p - end_p).length();
    }

    inline const distribution_bundle_t* get(const index_t &bi) const
    {
        return bundle_storage_->get(bi);
    }

    inline T sample(const point_t &p,
                    const typename inverse_sensor_model_t::Ptr &ivm) const
    {
        return sample(p, toBundleIndex(p), ivm);
    }

    inline T sample(const point_t &p,
                    const index_t &bi,
                    const typename inverse_sensor_model_t::Ptr &ivm) const
    {
        if (!ivm)
            throw std::runtime_error("[OccupancyGridMap]: inverse model not set");

        distribution_bundle_t *bundle = bundle_storage_->get(bi);

        auto sample = [&p, &ivm] (const distribution_t *d) {
            auto do_sample = [&p, &ivm, &d]() {
                const auto &handle = d;
                return handle->getDistribution() ?
                            handle->getDistribution()->sample(p) * handle->getOccupancy(ivm) : 0.0;
            };
            return d ? do_sample() : 0.0;
        };
        auto evaluate = [&p, &bundle, &sample]() {
            return 0.25 * (sample(bundle->at(0)) +
                           sample(bundle->at(1)) +
                           sample(bundle->at(2)) +
                           sample(bundle->at(3)));
        };
        return bundle ? evaluate() : 0.0;
    }

    inline T sampleNonNormalized(const point_t &p,
                                 const typename inverse_sensor_model_t::Ptr &ivm) const
    {
        return sampleNonNormalized(p, toBundleIndex(p), ivm);
    }

    inline T sampleNonNormalized(const point_t &p,
                                 const index_t &bi,
                                 const typename inverse_sensor_model_t::Ptr &ivm) const
    {
        if (!ivm)
            throw std::runtime_error("[OccupancyGridMap]: inverse model not set");

        distribution_bundle_t *bundle  = bundle_storage_->get(bi);

        auto sample = [&p, &ivm] (const distribution_t *d) {
            auto do_sample = [&p, &ivm, &d]() {
                const auto &handle = d;
                return handle->getDistribution() ?
                            handle->getDistribution()->sampleNonNormalized(p) * handle->getOccupancy(ivm) : 0.0;
            };
            return d ? do_sample() : 0.0;
        };
        auto evaluate = [&p, &bundle, &sample]() {
            return 0.25 * (sample(bundle->at(0)) +
                           sample(bundle->at(1)) +
                           sample(bundle->at(2)) +
                           sample(bundle->at(3)));
        };
        return bundle ? evaluate() : 0.0;
    }

    inline index_t getMinBundleIndex() const
    {
        return min_index_;
    }

    inline index_t getMaxBundleIndex() const
    {
        return max_index_;
    }

    inline const distribution_bundle_t* getDistributionBundle(const index_t &bi) const
    {
        return getAllocate(bi);
    }

    inline distribution_bundle_t* getDistributionBundle(const index_t &bi)
    {
        return getAllocate(bi);
    }

    inline T getBundleResolution() const
    {
        return bundle_resolution_;
    }

    inline T getResolution() const
    {
        return resolution_;
    }

    inline T getHeight() const
    {
        return (max_index_[1] - min_index_[1] + 1) * bundle_resolution_;
    }

    inline T getWidth() const
    {
        return (max_index_[0] - min_index_[0] + 1) * bundle_resolution_;
    }

    inline distribution_storage_array_t const & getStorages() const
    {
        return storage_;
    }

    template <typename Fn>
    inline void traverse(const Fn& function) const
    {
        return bundle_storage_->traverse(function);
    }

    inline void getBundleIndices(std::vector<index_t> &indices) const
    {
        auto add_index = [&indices](const index_t &i, const distribution_bundle_t &d) {
            indices.emplace_back(i);
        };
        bundle_storage_->traverse(add_index);
    }

    inline std::size_t getByteSize() const
    {
        return sizeof(*this) +
                bundle_storage_->byte_size() +
                storage_[0]->byte_size() +
                storage_[1]->byte_size() +
                storage_[2]->byte_size() +
                storage_[3]->byte_size();
    }

    inline virtual bool validate(const pose_t &p_w) const
    {
        const point_t p_m = m_T_w_ * p_w.translation();
        index_t i = {{static_cast<int>(std::floor(p_m(0) * bundle_resolution_)),
                      static_cast<int>(std::floor(p_m(1) * bundle_resolution_))}};

        return (i[0] >= min_index_[0]  && i[0] <= max_index_[0]) &&
                (i[1] >= min_index_[1]  && i[1] <= max_index_[1]);
    }

    inline void allocatePartiallyAllocatedBundles()
    {
        std::vector<index_t> bis;
        getBundleIndices(bis);

        using neighborhood_t = cis::operations::clustering::GridNeighborhoodStatic<std::tuple_size<index_t>::value, 3>;
        static constexpr neighborhood_t grid{};

        auto expand_distribution = [](const distribution_t* d) {
            if (!d || !d->getDistribution())
                return false;
            return d->getDistribution()->getN() >= 3;
        };

        for (const index_t &bi : bis) {
            const distribution_bundle_t *bundle  = bundle_storage_->get(bi);

            bool expand =
                    expand_distribution(bundle->at(0)) ||
                    expand_distribution(bundle->at(1)) ||
                    expand_distribution(bundle->at(2)) ||
                    expand_distribution(bundle->at(3));

            if (expand) {
                grid.visit([this, &bi](neighborhood_t::offset_t o) {
                    getAllocate({{bi[0]+o[0], bi[1]+o[1]}});
                });
            }
        }
    }

protected:
    const T                                    resolution_;
    const T                                    bundle_resolution_;
    const T                                    bundle_resolution_inv_;
    const transform_t                          w_T_m_;
    const transform_t                          m_T_w_;

    mutable index_t                            min_index_;
    mutable index_t                            max_index_;
    mutable distribution_storage_array_t       storage_;
    mutable distribution_bundle_storage_ptr_t  bundle_storage_;

    inline distribution_t* getAllocate(const distribution_storage_ptr_t &s,
                                       const index_t &i) const
    {
        distribution_t *d = s->get(i);
        return d ? d : &(s->insert(i, distribution_t()));
    }

    inline distribution_bundle_t *getAllocate(const index_t &bi) const
    {
        auto get_allocate = [this](const index_t &bi) {
            distribution_bundle_t *bundle = bundle_storage_->get(bi);

            auto allocate_bundle = [this, &bi]() {
                const int divx = cslibs_math::common::div(bi[0], 2);
                const int divy = cslibs_math::common::div(bi[1], 2);
                const int modx = cslibs_math::common::mod(bi[0], 2);
                const int mody = cslibs_math::common::mod(bi[1], 2);

                const index_t storage_0_index = {{divx,        divy}};
                const index_t storage_1_index = {{divx + modx, divy}};        /// shifted to the left
                const index_t storage_2_index = {{divx,        divy + mody}}; /// shifted to the bottom
                const index_t storage_3_index = {{divx + modx, divy + mody}}; /// shifted diagonally


                distribution_bundle_t b;
                b[0] = getAllocate(storage_[0], storage_0_index);
                b[1] = getAllocate(storage_[1], storage_1_index);
                b[2] = getAllocate(storage_[2], storage_2_index);
                b[3] = getAllocate(storage_[3], storage_3_index);

                updateIndices(bi);
                return &(bundle_storage_->insert(bi, b));
            };
            return bundle ? bundle : allocate_bundle();
        };
        return get_allocate(bi);
    }

    inline void updateFree(const index_t &bi) const
    {
        distribution_bundle_t *bundle = getAllocate(bi);
        bundle->at(0)->updateFree();
        bundle->at(1)->updateFree();
        bundle->at(2)->updateFree();
        bundle->at(3)->updateFree();
    }

    inline void updateFree(const index_t &bi,
                           const std::size_t &n) const
    {
        distribution_bundle_t *bundle = getAllocate(bi);
        bundle->at(0)->updateFree(n);
        bundle->at(1)->updateFree(n);
        bundle->at(2)->updateFree(n);
        bundle->at(3)->updateFree(n);
    }

    inline void updateOccupied(const index_t &bi,
                               const point_t &p) const
    {
        distribution_bundle_t *bundle = getAllocate(bi);
        bundle->at(0)->updateOccupied(p);
        bundle->at(1)->updateOccupied(p);
        bundle->at(2)->updateOccupied(p);
        bundle->at(3)->updateOccupied(p);
    }

    inline void updateOccupied(const index_t &bi,
                               const typename distribution_t::distribution_ptr_t &d) const
    {
        distribution_bundle_t *bundle = getAllocate(bi);
        bundle->at(0)->updateOccupied(d);
        bundle->at(1)->updateOccupied(d);
        bundle->at(2)->updateOccupied(d);
        bundle->at(3)->updateOccupied(d);
    }

    inline void updateIndices(const index_t &bi) const
    {
        min_index_ = std::min(min_index_, bi);
        max_index_ = std::max(max_index_, bi);
    }

    inline index_t toBundleIndex(const point_t &p_w) const
    {
        const point_t p_m = m_T_w_ * p_w;
        return {{static_cast<int>(std::floor(p_m(0) * bundle_resolution_inv_)),
                 static_cast<int>(std::floor(p_m(1) * bundle_resolution_inv_))}};
    }
};
}
}

#endif // CSLIBS_NDT_2D_DYNAMIC_MAPS_OCCUPANCY_GRIDMAP_HPP

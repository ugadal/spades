/*
 * path_extender.hpp
 *
 *  Created on: Mar 5, 2012
 *      Author: andrey
 */

#ifndef PATH_EXTENDER_HPP_
#define PATH_EXTENDER_HPP_

#include "extension_chooser.hpp"
#include "path_filter.hpp"

namespace path_extend {

class GraphCoverageMap: public PathListener {

public:
    typedef std::multiset <BidirectionalPath *> MapDataT;


protected:
    Graph& g_;

    std::map <EdgeId, MapDataT * > edgeCoverage_;

    MapDataT * empty_;

    virtual void EdgeAdded(EdgeId e, BidirectionalPath * path, int gap) {
        auto iter = edgeCoverage_.find(e);
        if (iter == edgeCoverage_.end()) {
            edgeCoverage_.insert(std::make_pair(e, new MapDataT()));
        }
        edgeCoverage_[e]->insert(path);
    }

    virtual void EdgeRemoved(EdgeId e, BidirectionalPath * path) {
        auto iter = edgeCoverage_.find(e);
        if (iter != edgeCoverage_.end()) {
            if (iter->second->count(path) == 0) {
                WARN("Error erasing path from coverage map");
            } else {
                auto entry = iter->second->find(path);
                iter->second->erase(entry);
            }
        }
    }

public:
    GraphCoverageMap(Graph& g_) : g_(g_), edgeCoverage_() {
        empty_ = new MapDataT();
    }

    virtual void FrontEdgeAdded(EdgeId e, BidirectionalPath * path, int gap) {
        EdgeAdded(e, path, gap);
    }

    virtual void BackEdgeAdded(EdgeId e, BidirectionalPath * path, int gap) {
        EdgeAdded(e, path, gap);
    }

    virtual void FrontEdgeRemoved(EdgeId e, BidirectionalPath * path) {
        EdgeRemoved(e, path);
    }

    virtual void BackEdgeRemoved(EdgeId e, BidirectionalPath * path) {
        EdgeRemoved(e, path);
    }

    MapDataT * GetEdgePaths(EdgeId e) const {
        auto iter = edgeCoverage_.find(e);
        if (iter != edgeCoverage_.end()) {
            return iter->second;
        }

        return empty_;
    }


    int GetCoverage(EdgeId e) const {
        return GetEdgePaths(e)->size();
    }


    bool IsCovered(EdgeId e) const {
        return GetCoverage(e) > 0;
    }

    bool IsCovered(const BidirectionalPath& path) const {
        for (size_t i = 0; i < path.Size(); ++i) {
            if (!IsCovered(path[i])) {
                return false;
            }
        }
        return true;
    }

    int GetCoverage(const BidirectionalPath& path) const {
        if (path.Empty()) {
            return 0;
        }

        int cov = GetCoverage(path[0]);
        for (size_t i = 1; i < path.Size(); ++i) {
            int currentCov = GetCoverage(path[i]);
            if (cov > currentCov) {
                cov = currentCov;
            }
        }

        return cov;
    }

    std::set<BidirectionalPath*> GetCoveringPaths(EdgeId e) const {
        auto mapData = GetEdgePaths(e);
        return std::set<BidirectionalPath*>(mapData->begin(), mapData->end());

    }

    std::set<BidirectionalPath*> GetCoveringPaths(const BidirectionalPath& path) const {
        std::set<BidirectionalPath*> result;

        if (!path.Empty()) {
            MapDataT * data;
            data = GetEdgePaths(path.Front());

            result.insert(data->begin(), data->end());

            for (size_t i = 1; i < path.Size(); ++i) {
                data = GetEdgePaths(path[i]);

                std::set<BidirectionalPath*> dataSet;
                dataSet.insert(data->begin(), data->end());

                for (auto iter = result.begin(); iter != result.end(); ) {
                    auto next = iter;
                    ++next;
                    if (dataSet.count(*iter) == 0) {
                        result.erase(iter);
                    }
                    iter = next;
                }
            }
        }

        return result;
    }

    int GetUniqueCoverage(EdgeId e) const {
        return GetCoveringPaths(e).size();
    }

    int GetUniqueCoverage(const BidirectionalPath& path) const {
        return GetCoveringPaths(path).size();
    }

    std::map <EdgeId, MapDataT * >::const_iterator begin() const {
        return edgeCoverage_.begin();
    }

    std::map <EdgeId, MapDataT * >::const_iterator end() const {
        return edgeCoverage_.end();
    }

    // DEBUG

    void PrintUncovered() const {
        DEBUG("Uncovered edges");
        for (auto iter = g_.SmartEdgeBegin(); !iter.IsEnd(); ++iter) {
            if (!IsCovered(*iter)) {
                DEBUG(g_.int_id(*iter) << " (" << g_.length(*iter) << ") ~ " << g_.int_id(g_.conjugate(*iter)) << " (" << g_.length(g_.conjugate(*iter)) << ")");
            }
        }
    }
};


class ShortLoopResolver {

protected:

    Graph& g_;

    bool GetLoopAndExit(BidirectionalPath& path, pair<EdgeId, EdgeId>& result) const {
        EdgeId e = path.Head();
        VertexId v = g_.EdgeEnd(e);

        if (g_.OutgoingEdgeCount(v) != 2) {
            return false;
        }

        EdgeId loop;
        EdgeId exit;
        auto edges = g_.OutgoingEdges(v);
        for (auto edge = edges.begin(); edge != edges.end(); ++edge) {
            if (g_.EdgeEnd(*edge) == g_.EdgeStart(e)) {
                loop = *edge;
            }
            else {
                exit = *edge;
            }
        }

        result = make_pair(loop, exit);
        return true;
    }

public:

    ShortLoopResolver(Graph& g): g_(g) {

    }

    virtual ~ShortLoopResolver() {

    }

    virtual void ResolveShortLoop(BidirectionalPath& path) = 0;

};


class SimpleLoopResolver: public ShortLoopResolver {

public:


    SimpleLoopResolver(Graph& g): ShortLoopResolver(g) {

    }

    virtual void ResolveShortLoop(BidirectionalPath& path) {
        pair<EdgeId, EdgeId> edges;

        if (GetLoopAndExit(path, edges)) {
            DEBUG("Resolving short loop...");
            path.Print();

            EdgeId e = path.Head();
            path.PushBack(edges.first);
            path.PushBack(e);
            path.PushBack(edges.second);
            DEBUG("Resolving short loop done");

            path.Print();
        }
    }
};


class LoopResolver: public ShortLoopResolver {

    static const size_t iter_ = 10;
    ExtensionChooser& chooser_;

public:


    LoopResolver(Graph& g, ExtensionChooser& chooser): ShortLoopResolver(g), chooser_(chooser) {

    }

    void MakeCycleStep(BidirectionalPath& path, EdgeId e) {
    	EdgeId pathEnd = path.Head();
		path.PushBack(e);
		path.PushBack(pathEnd);
    }

    void MakeBestChoice(BidirectionalPath& path, pair<EdgeId, EdgeId>& edges) {
         BidirectionalPath experiment(path);
         double maxWeight = chooser_.CountWeight(experiment, edges.second);
         double diff = maxWeight - chooser_.CountWeight(experiment, edges.first);
         size_t  maxIter = 0;
         for (size_t i = 1; i <= iter_; ++ i) {
        	 double weight = chooser_.CountWeight(experiment, edges.first);
        	 if (weight > 0) {
				 MakeCycleStep(experiment, edges.first);
				 weight = chooser_.CountWeight(experiment, edges.second);
				 double weight2 = chooser_.CountWeight(experiment, edges.first);
				 //INFO("now weight is " << weight << " dif w is: " << weight - weight2)
				 if (weight > maxWeight || (weight == maxWeight && weight - weight2 > diff) ) {
					 maxWeight = weight;
					 maxIter = i;
					 diff = weight - weight2;
				 }
        	 }
         }
         for (size_t i = 0; i < maxIter; ++ i) {
        	 MakeCycleStep(path, edges.first);
         }
         //INFO("Max number of iterations: " << maxIter);
         path.PushBack(edges.second);
    }

    virtual void ResolveShortLoop(BidirectionalPath& path) {
        pair<EdgeId, EdgeId> edges;

        if (GetLoopAndExit(path, edges)) {
            DEBUG("Resolving short loop...");
            //path.Print();
            MakeBestChoice(path, edges);
            DEBUG("Resolving short loop done");
            //path.Print();
        }
    }
};




class PathExtender {

protected:
    Graph& g_;

    size_t maxLoops_;

    bool investigateShortLoops_;

public:
    PathExtender(Graph & g): g_(g), maxLoops_(10), investigateShortLoops_(true)
    {
    }

    virtual ~PathExtender() {

    }

    virtual void GrowAll(PathContainer & paths, PathContainer * result) = 0;

    size_t getMaxLoops() const
    {
        return maxLoops_;
    }

    bool isInvestigateShortLoops() const
    {
        return investigateShortLoops_;
    }

    void setInvestigateShortLoops(bool investigateShortLoops)
    {
        this->investigateShortLoops_ = investigateShortLoops;
    }

    void setMaxLoops(size_t maxLoops)
    {
        if (maxLoops != 0) {
            this->maxLoops_ = maxLoops;
        }
    }

};



class CoveringPathExtender: public PathExtender {

protected:

    GraphCoverageMap coverageMap_;

    void SubscribeCoverageMap(BidirectionalPath * path) {
        path->Subscribe(&coverageMap_);
        for (size_t i = 0; i < path->Size(); ++i) {
            coverageMap_.BackEdgeAdded(path->At(i), path, path->GapAt(i));
        }
    }

    bool AllPathsCovered(PathContainer& paths) {
        for (size_t i = 0; i < paths.size(); ++i) {
            if (!coverageMap_.IsCovered(*paths.Get(i))) {
                return false;
            }
        }
        return true;
    }

    // DEBUG
    void VerifyMap(PathContainer * result) {
        //MAP VERIFICATION
        for (size_t i = 0; i < result->size(); ++i) {
            auto path = result->Get(i);
            for (size_t j = 0; j < path->Size(); ++j) {
                if (coverageMap_.GetCoveringPaths(path->At(j)).count(path) == 0) {
                    WARN("Inconsistent coverage map");
                }
            }

            path = result->GetConjugate(i);
            for (size_t j = 0; j < path->Size(); ++j) {
                if (coverageMap_.GetCoveringPaths(path->At(j)).count(path) == 0) {
                    WARN("Inconsistent coverage map");
                }
            }
        }
    }

    void GrowAll(PathContainer& paths, PathContainer& usedPaths, PathContainer * result) {

        for (size_t i = 0; i < paths.size(); ++i) {
            if (!coverageMap_.IsCovered(*paths.Get(i))) {
                usedPaths.AddPair(paths.Get(i), paths.GetConjugate(i));
                BidirectionalPath * path = new BidirectionalPath(*paths.Get(i));
                path->SetCurrentPathAsSeed();
                BidirectionalPath * conjugatePath = new BidirectionalPath(*paths.GetConjugate(i));
                conjugatePath->SetCurrentPathAsSeed();

                result->AddPair(path, conjugatePath);
                SubscribeCoverageMap(path);
                SubscribeCoverageMap(conjugatePath);

                if (!coverageMap_.IsCovered(*path) || !coverageMap_.IsCovered(*conjugatePath)) {
                    DEBUG("Paths are not covered after subsciption");
                }

                do {
                    //INFO("CHKF")
					path->CheckGrow();
					//INFO("FWD")
					GrowPath(*path);
					//verifyMap(result);
                    //INFO("CHKBW")
					conjugatePath->CheckGrow();
					//INFO("BCK")
					GrowPath(*conjugatePath);
					//verifyMap(result);
					//INFO("done")
					//path->Print();
                } while (conjugatePath->CheckPrevious() || path->CheckPrevious());

                if (!coverageMap_.IsCovered(*paths.Get(i)) || !coverageMap_.IsCovered(*paths.GetConjugate(i))) {
                    DEBUG("Seeds are not covered after growing");
                }
            }
        }
    }

    void RemoveSubpaths(PathContainer& usedPaths) {
        for (size_t i = 0; i < usedPaths.size(); ++i) {
            if (coverageMap_.GetUniqueCoverage(*usedPaths.Get(i)) > 1) {

                size_t seedId = usedPaths.Get(i)->GetId();
                size_t seedConjId = usedPaths.GetConjugate(i)->GetId();

                std::set<BidirectionalPath*> coveringPaths = coverageMap_.GetCoveringPaths(*(usedPaths.Get(i)));
                for (auto iter = coveringPaths.begin(); iter != coveringPaths.end(); ++iter) {

                    if ((*iter)->GetId() == seedId) {
                        bool otherSeedFound = false;
                        for (auto it = coveringPaths.begin(); it != coveringPaths.end(); ++it) {
                            if ((*it)->GetId() != seedId && (*it)->GetId() != seedConjId) {
                                otherSeedFound = true;
                                if ((*it)->Length() < (*iter)->Length()) {
                                    DEBUG("Covering path is shorter than the original seed path; seed path is to be removed");
                                }
                                if (!(*it)->Contains(**iter)) {
                                    DEBUG("Not subpaths");
                                    DEBUG((*iter)->Length() << " " << (*it)->Length());
                                    (*iter)->Print();
                                    (*it)->Print();
                                }
                            }
                        }

                        if (otherSeedFound) {
                            (*iter)->Clear();
                        }
                    }
                }
            }
        }
    }


    virtual void GrowPath(BidirectionalPath& path) = 0;

public:

    CoveringPathExtender(Graph& g_): PathExtender(g_), coverageMap_(g_) {
    }

    virtual void GrowAll(PathContainer& paths, PathContainer * result) {
        result->clear();
        PathContainer usedPaths;

        for (size_t i = 0; i < paths.size() && !AllPathsCovered(paths); i ++) {
		    GrowAll(paths, usedPaths, result);
		    RemoveSubpaths(usedPaths);
        }

        LengthPathFilter filter(g_, 0);
        filter.filter(*result);
    }

    GraphCoverageMap& GetCoverageMap() {
        return coverageMap_;
    }

};


class SimplePathExtender: public CoveringPathExtender {

protected:

    ExtensionChooser * extensionChooser_;

    LoopResolver loopResolver_;

    void FindFollowingEdges(BidirectionalPath& path, ExtensionChooser::EdgeContainer * result) {
        result->clear();
        auto edges = g_.OutgoingEdges(g_.EdgeEnd(path.Back()));

        result->reserve(edges.size());
        for (auto iter = edges.begin(); iter != edges.end(); ++iter) {
            result->push_back(EdgeWithDistance(*iter, 0));
        }
    }

    void ResolveShortLoop(BidirectionalPath& path) {

    }

    virtual void GrowPath(BidirectionalPath& path) {
        ExtensionChooser::EdgeContainer candidates;
        do {
            FindFollowingEdges(path, &candidates);
            candidates = extensionChooser_->Filter(path, candidates);

            if (candidates.size() == 1) {
                path.PushBack(candidates.back().e_, candidates.back().d_);

                if (investigateShortLoops_ && path.getLoopDetector().EdgeInShortLoop()) {
                    loopResolver_.ResolveShortLoop(path);
                }
            }

            if (path.getLoopDetector().IsCycled(maxLoops_)) {
                path.getLoopDetector().RemoveLoop();
                break;
            }

        } while (candidates.size() == 1);
    }

public:

    SimplePathExtender(Graph& g, ExtensionChooser * ec): CoveringPathExtender(g), extensionChooser_(ec), loopResolver_(g, *extensionChooser_) {
    }

};



class ScaffoldingPathExtender: public SimplePathExtender {

protected:

    ExtensionChooser * scaffoldingExtensionChooser_;

    std::vector<int> sizes_;

    ExtensionChooser::EdgeContainer sources_;


    void InitSources() {
        sources_.clear();

        for (auto iter = g_.SmartEdgeBegin(); !iter.IsEnd(); ++iter) {
            if (g_.IncomingEdgeCount(g_.EdgeStart(*iter)) == 0) {
                sources_.push_back(EdgeWithDistance(*iter, 0));
            }
        }
    }

    bool IsSink(EdgeId e)
	{
		return g_.OutgoingEdgeCount(g_.EdgeEnd(e)) == 0;
	}

    virtual void GrowPath(BidirectionalPath& path)
    {
        ExtensionChooser::EdgeContainer candidates;
        do {
        	FindFollowingEdges(path, &candidates);
			candidates = extensionChooser_->Filter(path, candidates);

			if (candidates.size() == 1) {
				path.PushBack(candidates.back().e_, candidates.back().d_);

	            if (investigateShortLoops_ && path.getLoopDetector().EdgeInShortLoop()) {
	                loopResolver_.ResolveShortLoop(path);
	            }
			}
			else if (IsSink(path.Back())) {
            	candidates = scaffoldingExtensionChooser_->Filter(path, sources_);

            	if (candidates.size() < sizes_.size()) {
            	    sizes_[candidates.size()] ++;
            	} else {
                    sizes_.resize(candidates.size() + 1, 0);
                    sizes_[candidates.size()] ++;
            	}
				if (candidates.size() == 1) {
					 DEBUG(candidates.size() << " " << g_.int_id(candidates[0].e_) << " Path id :" << path.GetId()<< "  Edge len : " << g_.length(candidates[0].e_))
                     path.PushBack(candidates.back().e_, candidates.back().d_);
					 //path.Print();
			    }
            }

			if (path.getLoopDetector().IsCycled(maxLoops_)) {
                //INFO("WTF: " << path.getLoopDetector().IsCycled(maxLoops_) << " " << maxLoops_);
                //path.Print();
				path.getLoopDetector().RemoveLoop();
				break;
			}
        }
        while (candidates.size() == 1);
    }


public:

    ScaffoldingPathExtender(Graph& g_, ExtensionChooser * usualEC, ExtensionChooser * scaffoldingEC): SimplePathExtender(g_, usualEC),
            scaffoldingExtensionChooser_(scaffoldingEC)  {
        InitSources();
    }

    virtual ~ScaffoldingPathExtender() {

    }

};




class ScaffoldingOnlyPathExtender: public SimplePathExtender {

protected:

    ExtensionChooser * scaffoldingExtensionChooser_;

    std::vector<int> sizes_;

    ExtensionChooser::EdgeContainer sources_;


    void InitSources() {
        sources_.clear();

        for (auto iter = g_.SmartEdgeBegin(); !iter.IsEnd(); ++iter) {
            if (g_.IncomingEdgeCount(g_.EdgeStart(*iter)) == 0) {
                sources_.push_back(EdgeWithDistance(*iter, 0));
            }
        }
        INFO("Found " << sources_.size() << " source edges");
    }

    bool IsSink(EdgeId e)
    {
        return g_.OutgoingEdgeCount(g_.EdgeEnd(e)) == 0;
    }

    virtual void GrowPath(BidirectionalPath& path)
    {
        ExtensionChooser::EdgeContainer candidates;
        do {
            candidates.clear();

            if (IsSink(path.Back())) {
                candidates = scaffoldingExtensionChooser_->Filter(path, sources_);

                if (candidates.size() < sizes_.size()) {
                    sizes_[candidates.size()] ++;
                } else {
                    sizes_.resize(candidates.size() + 1, 0);
                    sizes_[candidates.size()] ++;
                }
                if (candidates.size() == 1) {
                     DEBUG(candidates.size() << " " << g_.int_id(candidates[0].e_) << " Path id :" << path.GetId()<< "  Edge len : " << g_.length(candidates[0].e_))
                     path.PushBack(candidates.back().e_, candidates.back().d_);
                     //path.Print();
                }
            }

            if (path.getLoopDetector().IsCycled(maxLoops_)) {
                path.getLoopDetector().RemoveLoop();
                break;
            }
        }
        while (candidates.size() == 1);
    }


public:

    ScaffoldingOnlyPathExtender(Graph& g_,  ExtensionChooser * scaffoldingEC): SimplePathExtender(g_, 0),
            scaffoldingExtensionChooser_(scaffoldingEC)  {
        InitSources();
    }

    virtual ~ScaffoldingOnlyPathExtender() {

    }

};

}

#endif /* PATH_EXTENDER_HPP_ */

/* Authors: Lutong Wang and Bangqi Xu */
/*
 * Copyright (c) 2019, The Regents of the University of California
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the University nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _FLEX_GRID_GRAPH_H
#define _FLEX_GRID_GRAPH_H

#define GRIDGRAPHDRCCOSTSIZE 8

#include "frBaseTypes.h"
#include "FlexMazeTypes.h"
#include "frDesign.h"
#include "db/drObj/drPin.h"
#include "dr/FlexWavefront.h"
//#include <memory>
#include <map>
#include <iostream>


namespace fr {
  class FlexGridGraph {
  public:
    // constructors
    //FlexGridGraph() {}
    FlexGridGraph(frDesign* designIn): design(designIn)/*, routeBox()*/, bits(), /*mazeMarkers(),*/
                                       xCoords(), yCoords(), zCoords(), zHeights() {}
    // getters
    frTechObject* getTech() const {
      return design->getTech();
    }
    frDesign* getDesign() const {
      return design;
    }
    // getters
    // unsafe access, no check
    bool isAstarVisited(frMIdx x, frMIdx y, frMIdx z) const {
      return (getPrevAstarNodeDir(x, y, z) == frDirEnum::UNKNOWN);
    }
    // unsafe access, no check
    frDirEnum getPrevAstarNodeDir(frMIdx x, frMIdx y, frMIdx z) const {
      auto baseIdx = 3 * getIdx(x, y, z);
      return (frDirEnum)(((unsigned short)(prevDirs[baseIdx]    ) << 2) + 
                         ((unsigned short)(prevDirs[baseIdx + 1]) << 1) + 
                         ((unsigned short)(prevDirs[baseIdx + 2]) << 0));
    }
    // unsafe access, no check
    bool isSrc(frMIdx x, frMIdx y, frMIdx z) const {
      return srcs[getIdx(x, y, z)];
    }
    // unsafe access, no check
    bool isDst(frMIdx x, frMIdx y, frMIdx z) const {
      return dsts[getIdx(x, y, z)];
    }
    // unsafe access, no check
    //bool isBlocked(frMIdx x, frMIdx y, frMIdx z) const {
    //  return getBit(getIdx(x, y, z), 5);
    //}
    // unsafe access, no check
    bool isSVia(frMIdx x, frMIdx y, frMIdx z) const {
      return getBit(getIdx(x, y, z), 9);
    }
    // unsafe access, no check
    bool hasShapePlanar(frMIdx x, frMIdx y, frMIdx z) const {
      return getBit(getIdx(x, y, z), 10);
    }
    // unsafe access, no check
    bool hasShapeVia(frMIdx x, frMIdx y, frMIdx z) const {
      return getBit(getIdx(x, y, z), 11);
    }
    // unsafe access, no check
    bool hasGridCostE(frMIdx x, frMIdx y, frMIdx z) const {
      return getBit(getIdx(x, y, z), 12);
    }
    // unsafe access, no check
    bool hasGridCostN(frMIdx x, frMIdx y, frMIdx z) const {
      return getBit(getIdx(x, y, z), 13);
    }
    // unsafe access, no check
    bool hasGridCostU(frMIdx x, frMIdx y, frMIdx z) const {
      return getBit(getIdx(x, y, z), 14);
    }

    //const frBox& getRouteBox() const {
    //  return routeBox;
    //}
    void getBBox(frBox &in) const {
      if (xCoords.size() && yCoords.size()) {
        in.set(xCoords.front(), yCoords.front(), xCoords.back(), yCoords.back());
      }
    }
    void getDim(frMIdx &xDim, frMIdx &yDim, frMIdx &zDim) const {
      xDim = xCoords.size();
      yDim = yCoords.size();
      zDim = zCoords.size();
    }
    // unsafe access
    frPoint& getPoint(frPoint &in, frMIdx x, frMIdx y) const {
      in.set(xCoords[x], yCoords[y]);
      return in;
    }
    // unsafe access
    frLayerNum getLayerNum(frMIdx z) const {
      return zCoords[z];
    }
    bool hasMazeXIdx(frCoord in) const {
      return std::binary_search(xCoords.begin(), xCoords.end(), in);
    }
    bool hasMazeYIdx(frCoord in) const {
      return std::binary_search(yCoords.begin(), yCoords.end(), in);
    }
    bool hasMazeZIdx(frLayerNum in) const {
      return std::binary_search(zCoords.begin(), zCoords.end(), in);
    }
    bool hasIdx(const frPoint &p, frLayerNum lNum) const {
      return (hasMazeXIdx(p.x()) && hasMazeYIdx(p.y()) && hasMazeZIdx(lNum));
    }
    bool hasMazeIdx(const frPoint &p, frLayerNum lNum) const {
      return (hasMazeXIdx(p.x()) && hasMazeYIdx(p.y()) && hasMazeZIdx(lNum));
    }
    frMIdx getMazeXIdx(frCoord in) const {
      auto it = std::lower_bound(xCoords.begin(), xCoords.end(), in);
      return it - xCoords.begin();
    }
    frMIdx getMazeYIdx(frCoord in) const {
      auto it = std::lower_bound(yCoords.begin(), yCoords.end(), in);
      return it - yCoords.begin();
    }
    frMIdx getMazeZIdx(frLayerNum in) const {
      auto it = std::lower_bound(zCoords.begin(), zCoords.end(), in);
      return it - zCoords.begin();
    }
    FlexMazeIdx& getMazeIdx(FlexMazeIdx &mIdx, const frPoint &p, frLayerNum layerNum) const {
      mIdx.set(getMazeXIdx(p.x()), getMazeYIdx(p.y()), getMazeZIdx(layerNum));
      return mIdx;
    }
    // unsafe access, z always = 0
    void getIdxBox(FlexMazeIdx &mIdx1, FlexMazeIdx &mIdx2, const frBox &box) const {
      mIdx1.set(std::lower_bound(xCoords.begin(), xCoords.end(), box.left()  ) - xCoords.begin(),
                std::lower_bound(yCoords.begin(), yCoords.end(), box.bottom()) - yCoords.begin(),
                mIdx1.z());
      mIdx2.set(frMIdx(std::upper_bound(xCoords.begin(), xCoords.end(), box.right() ) - xCoords.begin()) - 1,
                frMIdx(std::upper_bound(yCoords.begin(), yCoords.end(), box.top()   ) - yCoords.begin()) - 1,
                mIdx2.z());
    }
    frCoord getZHeight(frMIdx in) const {
      return zHeights[in];
    }
    bool getZDir(frMIdx in) const {
      return zDirs[in];
    }
    bool hasEdge(frMIdx x, frMIdx y, frMIdx z, frDirEnum dir) const {
      correct(x, y, z, dir);
      if (isValid(x, y, z)) {
        auto idx = getIdx(x, y, z);
        switch (dir) {
          case frDirEnum::E:
            return getBit(idx, 0);
          case frDirEnum::N:
            return getBit(idx, 1);
          case frDirEnum::U:
            return getBit(idx, 2);
          default:
            return false;
        }
      } else {
        return false;
      }
    }
    bool hasGridCost(frMIdx x, frMIdx y, frMIdx z, frDirEnum dir) {
      bool sol = false;
      correct(x, y, z, dir);
      switch(dir) {
        case frDirEnum::E: 
          sol = hasGridCostE(x, y, z);
          break;
        case frDirEnum::N:
          sol = hasGridCostN(x, y, z);
          break;
        default: 
          sol = hasGridCostU(x, y, z);
      }
      return sol;
    }
    bool hasShapeCost(frMIdx x, frMIdx y, frMIdx z, frDirEnum dir) {
      bool sol = false;
      if (dir != frDirEnum::D && dir != frDirEnum::U) {
        reverse(x, y, z, dir);
        sol = hasShapePlanar(x, y, z);
      } else {
        correctU(x, y, z, dir);
        sol = hasShapeVia(x, y, z);
      }
      return sol;
    }
    bool hasDRCCost(frMIdx x, frMIdx y, frMIdx z, frDirEnum dir) {
      frUInt4 sol = 0;
      if (dir != frDirEnum::D && dir != frDirEnum::U) {
        reverse(x, y, z, dir);
        auto idx = getIdx(x, y, z);
        sol = (getBits(idx, 16, GRIDGRAPHDRCCOSTSIZE));
      } else {
        correctU(x, y, z, dir);
        auto idx = getIdx(x, y, z);
        sol = (getBits(idx, 24, GRIDGRAPHDRCCOSTSIZE));
      }
      return (sol);
    }
    bool hasMarkerCost(frMIdx x, frMIdx y, frMIdx z, frDirEnum dir) {
      frUInt4 sol = 0;
      if (dir != frDirEnum::D && dir != frDirEnum::U) {
        reverse(x, y, z, dir);
        auto idx = getIdx(x, y, z);
        sol = (getBits(idx, 32, GRIDGRAPHDRCCOSTSIZE));
      } else {
        correctU(x, y, z, dir);
        auto idx = getIdx(x, y, z);
        sol = (getBits(idx, 40, GRIDGRAPHDRCCOSTSIZE));
      }
      return (sol);
    }
    // unsafe access
    frCoord getEdgeLength(frMIdx x, frMIdx y, frMIdx z, frDirEnum dir) {
      frCoord sol = 0;
      correct(x, y, z, dir);
      //if (isValid(x, y, z, dir)) {
        switch (dir) {
          case frDirEnum::E:
            sol = xCoords[x+1] - xCoords[x];
            break;
          case frDirEnum::N:
            sol = yCoords[y+1] - yCoords[y];
            break;
          case frDirEnum::U:
            sol = zHeights[z+1] - zHeights[z];
            break;
          default:
            ;
        }
      //}
      return sol;
    }
    bool isEdgeInBox(frMIdx x, frMIdx y, frMIdx z, frDirEnum dir, const frBox &box, bool initDR) {
      bool sol = false;
      correct(x, y, z, dir);
      if (isValid(x, y, z, dir)) {
        auto x1 = x;
        auto y1 = y;
        auto z1 = z;
        reverse(x1, y1, z1, dir);
        frPoint pt, pt1;
        getPoint(pt, x, y);
        getPoint(pt1, x1, y1);
        if (box.contains(pt) && box.contains(pt1)) {
          // initDR must not use top and rightmost track
          if (initDR && 
              ((box.right() == pt.x() && box.right() == pt1.x()) ||
               (box.top()   == pt.y() && box.top()   == pt1.y()))) {
            sol = false;
          } else {
            sol = true;
          }
        } else {
          sol = false;
        }
      } else {
        sol = false;
      }
      return sol;
    }
    // setters
    bool addEdge(frMIdx x, frMIdx y, frMIdx z, frDirEnum dir, const frBox &box, bool initDR) {
      bool sol = false;
      if (!isEdgeInBox(x, y, z, dir, box, initDR)) {
        sol = false;
      } else {
        //cout <<"orig edge (" <<x <<", " <<y <<", " <<z <<", " <<int(dir) <<")" <<endl;
        correct(x, y, z, dir);
        //cout <<"corr edge (" <<x <<", " <<y <<", " <<z <<", " <<int(dir) <<")" <<endl;
        if (isValid(x, y, z, dir)) {
          auto idx = getIdx(x, y, z);
          switch (dir) {
            case frDirEnum::E:
              setBit(idx, 0);
              sol = true;
              break;
            case frDirEnum::N:
              setBit(idx, 1);
              sol = true;
              break;
            case frDirEnum::U:
              setBit(idx, 2);
              sol = true;
              break;
            default:
              ;
          }
        } else {
          //cout <<"not valid edge";
        }
      }
      return sol;
    }
    bool removeEdge(frMIdx x, frMIdx y, frMIdx z, frDirEnum dir) {
      bool sol = false;
      correct(x, y, z, dir);
      if (isValid(x, y, z, dir)) {
        auto idx = getIdx(x, y, z);
        switch (dir) {
          case frDirEnum::E:
            resetBit(idx, 0);
            sol = true;
            break;
          case frDirEnum::N:
            resetBit(idx, 1);
            sol = true;
            break;
          case frDirEnum::U:
            resetBit(idx, 2);
            sol = true;
            break;
          default:
            ;
        }
      }
      return sol;
    }
    void addDRCCostPlanar(frMIdx x, frMIdx y, frMIdx z) {
      auto idx = getIdx(x, y, z);
      addToBits(idx, 16, GRIDGRAPHDRCCOSTSIZE, 1);
    }
    void addDRCCostVia(frMIdx x, frMIdx y, frMIdx z) {
      auto idx = getIdx(x, y, z);
      addToBits(idx, 24, GRIDGRAPHDRCCOSTSIZE, 1);
    }
    void subDRCCostPlanar(frMIdx x, frMIdx y, frMIdx z) {
      auto idx = getIdx(x, y, z);
      subToBits(idx, 16, GRIDGRAPHDRCCOSTSIZE, 1);
    }
    void subDRCCostVia(frMIdx x, frMIdx y, frMIdx z) {
      auto idx = getIdx(x, y, z);
      subToBits(idx, 24, GRIDGRAPHDRCCOSTSIZE, 1);
    }
    void resetDRCCostPlanar(frMIdx x, frMIdx y, frMIdx z) {
      auto idx = getIdx(x, y, z);
      setBits(idx, 16, GRIDGRAPHDRCCOSTSIZE, 0);
    }
    void resetDRCCostVia(frMIdx x, frMIdx y, frMIdx z) {
      auto idx = getIdx(x, y, z);
      setBits(idx, 24, GRIDGRAPHDRCCOSTSIZE, 0);
    }
    void addMarkerCostPlanar(frMIdx x, frMIdx y, frMIdx z) {
      auto idx = getIdx(x, y, z);
      addToBits(idx, 32, GRIDGRAPHDRCCOSTSIZE, 10);
    }
    void addMarkerCostVia(frMIdx x, frMIdx y, frMIdx z) {
      auto idx = getIdx(x, y, z);
      addToBits(idx, 40, GRIDGRAPHDRCCOSTSIZE, 10);
    }
    bool decayMarkerCostPlanar(frMIdx x, frMIdx y, frMIdx z, float d) {
      auto idx = getIdx(x, y, z);
      int currCost = (getBits(idx, 32, GRIDGRAPHDRCCOSTSIZE));
      currCost *= d;
      currCost = std::max(0, currCost);
      setBits(idx, 32, GRIDGRAPHDRCCOSTSIZE, currCost);
      return (currCost == 0);
    }
    bool decayMarkerCostVia(frMIdx x, frMIdx y, frMIdx z, float d) {
      auto idx = getIdx(x, y, z);
      int currCost = (getBits(idx, 40, GRIDGRAPHDRCCOSTSIZE));
      currCost *= d;
      currCost = std::max(0, currCost);
      setBits(idx, 40, GRIDGRAPHDRCCOSTSIZE, currCost);
      return (currCost == 0);
    }
    //void resetMarkerCostPlanar(frMIdx x, frMIdx y, frMIdx z) {
    //  auto idx = getIdx(x, y, z);
    //  setBits(idx, 32, GRIDGRAPHDRCCOSTSIZE, 0);
    //}
    //void resetMarkerCostVia(frMIdx x, frMIdx y, frMIdx z) {
    //  auto idx = getIdx(x, y, z);
    //  setBits(idx, 40, GRIDGRAPHDRCCOSTSIZE, 0);
    //}
    //void resetCost(frMIdx x, frMIdx y, frMIdx z);

    // unsafe access, no idx check
    void setPrevAstarNodeDir(frMIdx x, frMIdx y, frMIdx z, frDirEnum dir) {
      auto baseIdx = 3 * getIdx(x, y, z);
      prevDirs[baseIdx]     = ((unsigned short)dir >> 2) & 1;
      prevDirs[baseIdx + 1] = ((unsigned short)dir >> 1) & 1;
      prevDirs[baseIdx + 2] = ((unsigned short)dir     ) & 1;
    }
    // unsafe access, no idx check
    void setSrc(frMIdx x, frMIdx y, frMIdx z) {
      srcs[getIdx(x, y, z)] = 1;
    }
    void setSrc(const FlexMazeIdx &mi) {
      srcs[getIdx(mi.x(), mi.y(), mi.z())] = 1;
    }
    // unsafe access, no idx check
    void setDst(frMIdx x, frMIdx y, frMIdx z) {
      dsts[getIdx(x, y, z)] = 1;
    }
    void setDst(const FlexMazeIdx &mi) {
      dsts[getIdx(mi.x(), mi.y(), mi.z())] = 1;
    }
    // unsafe access, no idx check
    //void setBlocked(frMIdx x, frMIdx y, frMIdx z) {
    //  setBit(getIdx(x, y, z), 5);
    //}
    // unsafe access
    void setSVia(frMIdx x, frMIdx y, frMIdx z) {
      setBit(getIdx(x, y, z), 9);
    }
    void setShapePlanar(frMIdx x, frMIdx y, frMIdx z) {
      setBit(getIdx(x, y, z), 10);
    }
    void setShapeVia(frMIdx x, frMIdx y, frMIdx z) {
      setBit(getIdx(x, y, z), 11);
    }
    void setGridCostE(frMIdx x, frMIdx y, frMIdx z) {
      setBit(getIdx(x, y, z), 12);
    }
    void setGridCostN(frMIdx x, frMIdx y, frMIdx z) {
      setBit(getIdx(x, y, z), 13);
    }
    void setGridCostU(frMIdx x, frMIdx y, frMIdx z) {
      setBit(getIdx(x, y, z), 14);
    }
    // unsafe access, no idx check
    void resetSrc(frMIdx x, frMIdx y, frMIdx z) {
      srcs[getIdx(x, y, z)] = 0;
    }
    void resetSrc(const FlexMazeIdx &mi) {
      srcs[getIdx(mi.x(), mi.y(), mi.z())] = 0;
    }
    // unsafe access, no idx check
    void resetDst(frMIdx x, frMIdx y, frMIdx z) {
      dsts[getIdx(x, y, z)] = 0;
    }
    void resetDst(const FlexMazeIdx &mi) {
      dsts[getIdx(mi.x(), mi.y(), mi.z())] = 0;
    }
    // unsafe access, no idx check
    //void resetBlocked(frMIdx x, frMIdx y, frMIdx z) {
    //  resetBit(getIdx(x, y, z), 5);
    //}
    // unsafe access
    //void resetSVia(frMIdx x, frMIdx y, frMIdx z) {
    //  resetBit(getIdx(x, y, z), 9);
    //}
    //void resetShapePlanar(frMIdx x, frMIdx y, frMIdx z) {
    //  resetBit(getIdx(x, y, z), 10);
    //}
    void resetShapeVia(frMIdx x, frMIdx y, frMIdx z) {
      resetBit(getIdx(x, y, z), 11);
    }
    //void resetGridCostE(frMIdx x, frMIdx y, frMIdx z) {
    //  resetBit(getIdx(x, y, z), 12);
    //}
    //void resetGridCostN(frMIdx x, frMIdx y, frMIdx z) {
    //  resetBit(getIdx(x, y, z), 13);
    //}
    //void resetGridCostU(frMIdx x, frMIdx y, frMIdx z) {
    //  resetBit(getIdx(x, y, z), 14);
    //}

    //void setRouteBox(const frBox &in) const {
    //  routeBox.set(in);
    //}

    // functions
    void init(const frBox &routeBBox, const frBox &extBBox,
              std::map<frCoord, std::map<frLayerNum, frTrackPattern* > > &xMap,
              std::map<frCoord, std::map<frLayerNum, frTrackPattern* > > &yMap,
              bool initDR);
    void print();
    void resetStatus();
    void resetPrevNodeDir();
    void resetSrc();
    void resetDst();
    bool search(std::vector<FlexMazeIdx> &connComps, drPin* nextPin, std::vector<FlexMazeIdx> &path,
                FlexMazeIdx &ccMazeIdx1, FlexMazeIdx &ccMazeIdx2);
    void setCost(frUInt4 drcCostIn, frUInt4 markerCostIn) {
      ggDRCCost    = drcCostIn;
      ggMarkerCost = markerCostIn;
    }
    frCoord getHalfViaEncArea(frMIdx z, bool isLayer1) const {
      return (isLayer1 ? halfViaEncArea[z].first: halfViaEncArea[z].second);
    }

  protected:
    frDesign* design;
    //frBox     routeBox;
    // new
    // [0] hasEEdge; [1] hasNEdge; [2] hasUpEdge
    // [3] empty;    [4] empty;    [5] empty
    // [6] empty;    [7] empty;    [8] empty
    // [9] hasSpecialVia
    // [10] shape X cost; [11] shape U cost; 
    // [12] is W/E on grid; [13] is N/S on grid; [14] is U on grid
    // [23-16] quick drc X cost; [31-24] quick drc U cost
    // [39-32] markerdrc X cost; [47-40] markerdrc U cost
    //frVector<unsigned long long>      bits;
    frVector<unsigned long long>               bits;
    std::vector<bool>                          prevDirs;
    std::vector<bool>                          srcs;
    std::vector<bool>                          dsts;
    //std::vector<bool>                          gridsE;
    //std::vector<bool>                          gridsN;
    frVector<frCoord>                          xCoords;
    frVector<frCoord>                          yCoords;
    frVector<frLayerNum>                       zCoords;
    frVector<frCoord>                          zHeights; // accumulated Z diff
    std::vector<bool>                          zDirs; // is horz dir
    frUInt4                                    ggDRCCost;
    frUInt4                                    ggMarkerCost;
    // temporary variables
    FlexWavefront                              wavefront;
    std::vector<std::pair<frCoord, frCoord> >  halfViaEncArea; // std::pair<layer1area, layer2area>

    // internal getters
    bool getBit(frMIdx idx, frMIdx pos) const {
      return (bits[idx] >> pos ) & 1;
    }
    frUInt4 getBits(frMIdx idx, frMIdx pos, frUInt4 length) const {
      auto tmp = bits[idx] & (((1ull << length) - 1) << pos); // mask
      //auto tmp = bits[idx] & (((1u << length) - 1) << pos); // mask
      return tmp >> pos;
    }
    frMIdx getIdx(frMIdx xIdx, frMIdx yIdx, frMIdx zIdx) const {
      ////auto sol = xIdx + yIdx * xCoords.size() + zIdx * xCoords.size() * yCoords.size();
      ////if (sol >= 0 && sol < bits.size()) {
      //  return xIdx + yIdx * xCoords.size() + zIdx * xCoords.size() * yCoords.size();
      ////} else {
      ////  std::cout <<"Error: currIdx x/y/z " <<xIdx <<"/" <<yIdx <<"/" <<zIdx <<" " <<sol <<" is illegal" <<std::endl <<std::flush;
      ////  throw std::runtime_error("error");
      ////}
      
      //      (isZDir == H)     
      return (getZDir(zIdx)) ? (xIdx + yIdx * xCoords.size() + zIdx * xCoords.size() * yCoords.size()): 
                               (yIdx + xIdx * yCoords.size() + zIdx * xCoords.size() * yCoords.size());
    }
    // internal setters
    void setBit(frMIdx idx, frMIdx pos) {
      bits[idx] |= 1 << pos;
    }
    void resetBit(frMIdx idx, frMIdx pos) {
      bits[idx] &= ~(1 << pos);
    }
    void addToBits(frMIdx idx, frMIdx pos, frUInt4 length, frUInt4 val) {
      auto tmp = getBits(idx, pos, length) + val;
      tmp = (tmp > (1u << length)) ? (1u << length) : tmp;
      setBits(idx, pos, length, tmp);
    }
    void subToBits(frMIdx idx, frMIdx pos, frUInt4 length, frUInt4 val) {
      int tmp = (int)getBits(idx, pos, length) - (int)val;
      tmp = (tmp < 0) ? 0 : tmp;
      setBits(idx, pos, length, tmp);
    }
    void setBits(frMIdx idx, frMIdx pos, frUInt4 length, frUInt4 val) {
      //std::cout <<"setBits (idx/pos/len/val) " <<idx <<" " <<pos <<" " <<length <<" " <<val <<std::endl;
      bits[idx] &= ~(((1ull << length) - 1) << pos); // clear related bits to 0
      bits[idx] |= ((unsigned long long)val & ((1ull << length) - 1)) << pos; // only get last length bits of val
      //bits[idx] &= ~(((1u << length) - 1) << pos); // clear related bits to 0
      //bits[idx] |= (val & ((1u << length) - 1)) << pos; // only get last length bits of val
    }

    // internal utility
    void correct(frMIdx &x, frMIdx &y, frMIdx &z, frDirEnum &dir) const {
      switch (dir) {
        case frDirEnum::W:
          x--;
          dir = frDirEnum::E;
          break;
        case frDirEnum::S:
          y--;
          dir = frDirEnum::N;
          break;
        case frDirEnum::D:
          z--;
          dir = frDirEnum::U;
          break;
        default:
          ;
      }
      return;
    }
    void correctU(frMIdx &x, frMIdx &y, frMIdx &z, frDirEnum &dir) const {
      switch (dir) {
        case frDirEnum::D:
          z--;
          dir = frDirEnum::U;
          break;
        default:
          ;
      }
      return;
    }
    void reverse(frMIdx &x, frMIdx &y, frMIdx &z, frDirEnum &dir) const {
      switch (dir) {
        case frDirEnum::E:
          x++;
          dir = frDirEnum::W;
          break;
        case frDirEnum::S:
          y--;
          dir = frDirEnum::N;
          break;
        case frDirEnum::W:
          x--;
          dir = frDirEnum::E;
          break;
        case frDirEnum::N:
          y++;
          dir = frDirEnum::S;
          break;
        case frDirEnum::U:
          z++;
          dir = frDirEnum::D;
          break;
        case frDirEnum::D:
          z--;
          dir = frDirEnum::U;
          break;
        default:
          ;
      }
      return;
    }
    void getPrevGrid(frMIdx &gridX, frMIdx &gridY, frMIdx &gridZ, const frDirEnum dir) const;
    void getNextGrid(frMIdx &gridX, frMIdx &gridY, frMIdx &gridZ, const frDirEnum dir);
    bool isValid(frMIdx x, frMIdx y, frMIdx z) const {
      if (x < 0 || y < 0 || z < 0 ||
          x >= (frMIdx)xCoords.size() || y >= (frMIdx)yCoords.size() || z >= (frMIdx)zCoords.size()) {
        return false;
      } else {
        return true;
      }
    }
    bool isValid(frMIdx x, frMIdx y, frMIdx z, frDirEnum dir) const {
      auto sol = isValid(x, y, z);
      reverse(x, y, z, dir);
      return sol && isValid(x, y, z);
    }
    //bool isOpposite(const frDirEnum &dir1, const frDirEnum &dir2) const {
    //  return ((int)dir1 + (int)dir2 == OPPOSITEDIR);
    //}
    // internal init utility
    void initTracks(std::map<frCoord, std::map<frLayerNum, frTrackPattern* > > &horLoc2TrackPatterns,
                    std::map<frCoord, std::map<frLayerNum, frTrackPattern* > > &vertLoc2TrackPatterns,
                    std::map<frLayerNum, frPrefRoutingDirEnum> &layerNum2PreRouteDir,
                    const frBox &bbox);
    void initGrids(const std::map<frCoord, std::map<frLayerNum, frTrackPattern* > > &xMap,
                   const std::map<frCoord, std::map<frLayerNum, frTrackPattern* > > &yMap,
                   const std::map<frLayerNum, frPrefRoutingDirEnum> &zMap);
    void initEdges(const std::map<frCoord, std::map<frLayerNum, frTrackPattern* > > &xMap,
                   const std::map<frCoord, std::map<frLayerNum, frTrackPattern* > > &yMap,
                   const std::map<frLayerNum, frPrefRoutingDirEnum> &zMap,
                   const frBox &bbox, bool initDR);
    frCost getEstCost(const FlexMazeIdx &src, const FlexMazeIdx &dstMazeIdx1, const FlexMazeIdx &dstMazeIdx2, const frDirEnum &dir);
    frCost getNextPathCost(const FlexWavefrontGrid &currGrid, const frDirEnum &dir);
    frDirEnum getLastDir(const std::bitset<WAVEFRONTBITSIZE> &buffer);
    void traceBackPath(const FlexWavefrontGrid &currGrid, std::vector<FlexMazeIdx> &path, 
                       std::vector<FlexMazeIdx> &root, FlexMazeIdx &ccMazeIdx1, FlexMazeIdx &ccMazeIdx2);
    void expandWavefront(FlexWavefrontGrid &currGrid, const FlexMazeIdx &dstMazeIdx1, const FlexMazeIdx &dstMazeIdx2);
    bool isExpandable(const FlexWavefrontGrid &currGrid, frDirEnum dir);
    //bool isOpposite(const frDirEnum &dir1, const frDirEnum &dir2);
    FlexMazeIdx getTailIdx(const FlexMazeIdx &currIdx, const FlexWavefrontGrid &currGrid);
    void expand(FlexWavefrontGrid &currGrid, const frDirEnum &dir, const FlexMazeIdx &dstMazeIdx1, const FlexMazeIdx &dstMazeIdx2);
  };
}




#endif

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

#include <cmath>
#include "ta/FlexTA.h"

using namespace std;
using namespace fr;

inline frCoord FlexTAWorker::box2boxDistSquare(const frBox &box1, const frBox &box2, frCoord &dx, frCoord &dy) {
  dx = max(max(box1.left(), box2.left())     - min(box1.right(), box2.right()), 0);
  dy = max(max(box1.bottom(), box2.bottom()) - min(box1.top(), box2.top()),     0);
  return dx * dx + dy * dy;
}

// must be current TA layer
void FlexTAWorker::modMinSpacingCostPlanar(const frBox &box, frLayerNum lNum, taPinFig* fig, bool isAddCost, set<taPin*, frBlockObjectComp> *pinS) {
  //bool enableOutput = true;
  bool enableOutput = false;
  double dbu = getDesign()->getTopBlock()->getDBUPerUU();
  // obj1 = curr obj
  frCoord width1  = box.width();
  frCoord length1 = box.length();
  // obj2 = other obj
  // layer default width
  frCoord width2     = getDesign()->getTech()->getLayer(lNum)->getWidth();
  frCoord halfwidth2 = width2 / 2;
  // spacing value needed
  frCoord bloatDist = 0;
  auto con = getDesign()->getTech()->getLayer(lNum)->getMinSpacing();
  if (con) {
    if (con->typeId() == frConstraintTypeEnum::frcSpacingConstraint) {
      bloatDist = static_cast<frSpacingConstraint*>(con)->getMinSpacing();
    } else if (con->typeId() == frConstraintTypeEnum::frcSpacingTablePrlConstraint) {
      bloatDist = static_cast<frSpacingTablePrlConstraint*>(con)->find(max(width1, width2), length1);
    } else if (con->typeId() == frConstraintTypeEnum::frcSpacingTableTwConstraint) {
      bloatDist = static_cast<frSpacingTableTwConstraint*>(con)->find(width1, width2, length1);
    } else {
      cout <<"Warning: min spacing rule not supporterd" <<endl;
      return;
    }
  } else {
    cout <<"Warning: no min spacing rule" <<endl;
    return;
  }
  frCoord bloatDistSquare = bloatDist * bloatDist;

  bool isH = (getDir() == frPrefRoutingDirEnum::frcHorzPrefRoutingDir);

  // now assume track in H direction
  frCoord boxLow  = isH ? box.bottom() : box.left();
  frCoord boxHigh = isH ? box.top()    : box.right();
  frCoord boxLeft = isH ? box.left()   : box.bottom();
  frCoord boxRight= isH ? box.right()  : box.top();
  frBox box1(boxLeft, boxLow, boxRight, boxHigh);

  int idx1, idx2;
  getTrackIdx(boxLow - bloatDist - halfwidth2 + 1, boxHigh + bloatDist + halfwidth2 - 1, lNum, idx1, idx2);

  frBox box2(-halfwidth2, -halfwidth2, halfwidth2, halfwidth2);
  frTransform xform;
  frCoord dx, dy;
  auto &trackLocs = getTrackLocs(lNum);
  auto &workerRegionQuery = getWorkerRegionQuery();
  for (int i = idx1; i <= idx2; i++) {
    //cout <<"@@@ " <<i <<endl <<flush;
    auto trackLoc = trackLocs[i];
    xform.set(frPoint(boxLeft, trackLoc));
    box2.transform(xform);
    box2boxDistSquare(box1, box2, dx, dy);
    frCoord maxX = (frCoord)(sqrt(1.0 * bloatDistSquare - 1.0 * dy * dy));
    if (maxX * maxX + dy * dy == bloatDistSquare) {
      maxX = max(0, maxX - 1);
    }
    frCoord blockLeft  = boxLeft  - maxX - halfwidth2;
    frCoord blockRight = boxRight + maxX + halfwidth2;
    frBox tmpBox;
    if (isH) {
      tmpBox.set(blockLeft, trackLoc, blockRight, trackLoc);
    } else {
      tmpBox.set(trackLoc, blockLeft, trackLoc, blockRight);
    }
    if (isAddCost) {
      workerRegionQuery.addCost(tmpBox, lNum, fig, con);
      if (pinS) {
        workerRegionQuery.query(tmpBox, lNum, *pinS);
      }
      if (enableOutput) {
        cout <<"  add minSpc planar@("
             <<tmpBox.left()  / dbu <<", " <<tmpBox.bottom() / dbu <<") (" 
             <<tmpBox.right() / dbu <<", " <<tmpBox.top()    / dbu <<") " 
             <<getDesign()->getTech()->getLayer(lNum)->getName() <<endl <<flush;
      }
    } else {
      workerRegionQuery.removeCost(tmpBox, lNum, fig, con);
      if (pinS) {
        workerRegionQuery.query(tmpBox, lNum, *pinS);
      }
      if (enableOutput) {
        cout <<"  sub minSpc planar@(" 
             <<tmpBox.left()  / dbu <<", " <<tmpBox.bottom() / dbu <<") (" 
             <<tmpBox.right() / dbu <<", " <<tmpBox.top()    / dbu <<") " 
             <<getDesign()->getTech()->getLayer(lNum)->getName() <<endl <<flush;
      }
    }
  }
}

// given a shape on any routing layer n, block via @(n+1) if isUpperVia is true
void FlexTAWorker::modMinSpacingCostVia(const frBox &box, frLayerNum lNum, taPinFig* fig, bool isAddCost, bool isUpperVia, bool isCurrPs, 
                                        set<taPin*, frBlockObjectComp> *pinS) {
  //bool enableOutput = true;
  bool enableOutput = false;
  double dbu = getDesign()->getTopBlock()->getDBUPerUU();
  // obj1 = curr obj
  frCoord width1  = box.width();
  frCoord length1 = box.length();
  // obj2 = other obj
  // default via dimension
  frViaDef* viaDef = nullptr;
  frLayerNum cutLNum = 0;
  if (isUpperVia) {
    viaDef = (lNum < getDesign()->getTech()->getTopLayerNum()) ? 
             getDesign()->getTech()->getLayer(lNum+1)->getDefaultViaDef() : 
             nullptr;
    cutLNum = lNum + 1;
  } else {
    viaDef = (lNum > getDesign()->getTech()->getBottomLayerNum()) ? 
             getDesign()->getTech()->getLayer(lNum-1)->getDefaultViaDef() : 
             nullptr;
    cutLNum = lNum - 1;
  }
  if (viaDef == nullptr) {
    return;
  }
  frVia via(viaDef);
  frBox viaBox(0,0,0,0);
  if (isUpperVia) {
    via.getLayer1BBox(viaBox);
  } else {
    via.getLayer2BBox(viaBox);
  }
  frCoord width2  = viaBox.width();
  frCoord length2 = viaBox.length();

  bool isH = (getDir() == frPrefRoutingDirEnum::frcHorzPrefRoutingDir);
  frLayerNum followTrackLNum = (int)getDesign()->getTech()->getBottomLayerNum() - 1;
  if (cutLNum - 1 >= getDesign()->getTech()->getBottomLayerNum() && 
      getDesign()->getTech()->getLayer(cutLNum - 1)->getType() == frLayerTypeEnum::ROUTING &&
      getDesign()->getTech()->getLayer(cutLNum - 1)->getDir() == getDir()) {
    followTrackLNum = cutLNum - 1;
  } else if (cutLNum + 1 <= getDesign()->getTech()->getTopLayerNum() && 
             getDesign()->getTech()->getLayer(cutLNum + 1)->getType() == frLayerTypeEnum::ROUTING &&
             getDesign()->getTech()->getLayer(cutLNum + 1)->getDir() == getDir()) {
    followTrackLNum = cutLNum + 1;
  } else {
    cout <<"Warning: via layer connected to non-routing layer, skipped in modMinSpacingCostVia" <<endl;
    return;
  }

  // spacing value needed
  frCoord bloatDist = 0;
  auto con = getDesign()->getTech()->getLayer(lNum)->getMinSpacing();
  if (con) {
    if (con->typeId() == frConstraintTypeEnum::frcSpacingConstraint) {
      bloatDist = static_cast<frSpacingConstraint*>(con)->getMinSpacing();
    } else if (con->typeId() == frConstraintTypeEnum::frcSpacingTablePrlConstraint) {
      bloatDist = static_cast<frSpacingTablePrlConstraint*>(con)->find(max(width1, width2), isCurrPs ? length2 : min(length1, length2));
    } else if (con->typeId() == frConstraintTypeEnum::frcSpacingTableTwConstraint) {
      bloatDist = static_cast<frSpacingTableTwConstraint*>(con)->find(width1, width2, isCurrPs ? length2 : min(length1, length2));
    } else {
      cout <<"Warning: min spacing rule not supporterd" <<endl;
      return;
    }
  } else {
    cout <<"Warning: no min spacing rule" <<endl;
    return;
  }
  // other obj eol spc to curr obj
  // no need to bloat eolWithin because eolWithin always < minSpacing
  //frCoord bloatDistEolX = 0;
  //frCoord bloatDistEolY = 0;
  //for (auto con: getDesign()->getTech()->getLayer(lNum)->getEolSpacing()) {
  //  auto eolSpace  = con->getMinSpacing();
  //  auto eolWidth  = con->getEolWidth();
  //  // eol up and down
  //  if (viaBox.right() - viaBox.left() < eolWidth) {
  //    bloatDistEolY = max(bloatDistEolY, eolSpace);
  //  } 
  //  // eol left and right
  //  if (viaBox.top() - viaBox.bottom() < eolWidth) {
  //    bloatDistEolX = max(bloatDistEolX, eolSpace);
  //  }
  //}
  //frCoord bloatDistSquare = bloatDist * bloatDist;

  int idx1, idx2;
  if (isH) {
    getTrackIdx(box.bottom() - bloatDist - (viaBox.top() - 0) + 1, 
                box.top()    + bloatDist + (0 - viaBox.bottom()) - 1,
                followTrackLNum, idx1, idx2);
  } else {
    getTrackIdx(box.left()   - bloatDist - (viaBox.right() - 0) + 1, 
                box.right()  + bloatDist + (0 - viaBox.left()) - 1,
                followTrackLNum, idx1, idx2);
  }

  auto &trackLocs = getTrackLocs(followTrackLNum);
  auto &workerRegionQuery = getWorkerRegionQuery();
  frBox tmpBx;
  frTransform xform;
  frCoord dx, dy, prl;
  //frCoord distSquare;
  frCoord reqDist = 0;
  frCoord maxX, blockLeft, blockRight;
  frBox blockBox;
  for (int i = idx1; i <= idx2; i++) {
    //cout <<"@@@ " <<i <<endl <<flush;
    auto trackLoc = trackLocs[i];
    if (isH) {
      xform.set(frPoint(box.left(), trackLoc));
    } else {
      xform.set(frPoint(trackLoc, box.bottom()));
    }
    tmpBx.set(viaBox);
    tmpBx.transform(xform);
    //distSquare = box2boxDistSquare(box, tmpBx, dx, dy);
    box2boxDistSquare(box, tmpBx, dx, dy);
    if (isH) { // track is horizontal
      if (dy > 0) { // via at the bottom of box
        if (isCurrPs) { // prl maxed out to be viaBox
          prl = viaBox.right() - viaBox.left();
        } else { // prl maxed out to be smaller of box and viaBox
          prl = min(box.right() - box.left(), viaBox.right() - viaBox.left());
        }
      // via at the side of box
      } else {
        if (isCurrPs) { // prl maxed out to be viaBox
          prl = viaBox.top() - viaBox.bottom();
        } else { // prl maxed out to be smaller of box and viaBox
          prl = min(box.top() - box.bottom(), viaBox.top() - viaBox.bottom());
        }
      }
    } else { // track is vertical
      if (dx > 0) { // via at the bottom of box
        if (isCurrPs) { // prl maxed out to be viaBox
          prl = viaBox.top() - viaBox.bottom();
        } else { // prl maxed out to be smaller of box and viaBox
          prl = min(box.top() - box.bottom(), viaBox.top() - viaBox.bottom());
        }
      // via at the side of box
      } else {
        if (isCurrPs) { // prl maxed out to be viaBox
          prl = viaBox.right() - viaBox.left();
        } else { // prl maxed out to be smaller of box and viaBox
          prl = min(box.right() - box.left(), viaBox.right() - viaBox.left());
        }
      }
    }
    
    if (con->typeId() == frConstraintTypeEnum::frcSpacingConstraint) {
      reqDist = static_cast<frSpacingConstraint*>(con)->getMinSpacing();
    } else if (con->typeId() == frConstraintTypeEnum::frcSpacingTablePrlConstraint) {
      reqDist = static_cast<frSpacingTablePrlConstraint*>(con)->find(max(width1, width2), prl);
    } else if (con->typeId() == frConstraintTypeEnum::frcSpacingTableTwConstraint) {
      reqDist = static_cast<frSpacingTableTwConstraint*>(con)->find(width1, width2, prl);
    }

    if (isH) {
      if (dy >= reqDist) {
        continue;
      }
      maxX = (frCoord)(sqrt(1.0 * reqDist * reqDist - 1.0 * dy * dy));
      if (maxX * maxX + dy * dy == reqDist * reqDist) {
        maxX = max(0, maxX - 1);
      }
      blockLeft  = box.left()  - maxX - (viaBox.right() - 0);
      blockRight = box.right() + maxX + (0- viaBox.left());

      blockBox.set(blockLeft, trackLoc, blockRight, trackLoc);
    } else {
      if (dx >= reqDist) {
        continue;
      }
      maxX = (frCoord)(sqrt(1.0 * reqDist * reqDist - 1.0 * dx * dx));
      if (maxX * maxX + dx * dx == reqDist * reqDist) {
        maxX = max(0, maxX - 1);
      }
      blockLeft  = box.bottom() - maxX - (viaBox.top() - 0);
      blockRight = box.top()    + maxX + (0- viaBox.bottom());

      blockBox.set(trackLoc, blockLeft, trackLoc, blockRight);
    }

    if (isAddCost) {
      workerRegionQuery.addCost(blockBox, cutLNum, fig, con);
      if (pinS) {
        workerRegionQuery.query(blockBox, cutLNum, *pinS);
      }
      if (enableOutput) {
        cout <<"  add minSpc via@("
             <<blockBox.left()  / dbu <<", " <<blockBox.bottom() / dbu <<") (" 
             <<blockBox.right() / dbu <<", " <<blockBox.top()    / dbu <<") " 
             <<getDesign()->getTech()->getLayer(cutLNum)->getName() <<endl <<flush;
      }
    } else {
      workerRegionQuery.removeCost(blockBox, cutLNum, fig, con);
      if (pinS) {
        workerRegionQuery.query(blockBox, cutLNum, *pinS);
      }
      if (enableOutput) {
        cout <<"  sub minSpc via@(" 
             <<blockBox.left()  / dbu <<", " <<blockBox.bottom() / dbu <<") (" 
             <<blockBox.right() / dbu <<", " <<blockBox.top()    / dbu <<") " 
             <<getDesign()->getTech()->getLayer(cutLNum)->getName() <<endl <<flush;
      }
    }
  }

}

void FlexTAWorker::modCutSpacingCost(const frBox &box, frLayerNum lNum, taPinFig* fig, bool isAddCost, set<taPin*, frBlockObjectComp> *pinS) {
  bool enableOutput = false;
  //bool enableOutput = true;
  double dbu = getDesign()->getTopBlock()->getDBUPerUU();
  if (!getDesign()->getTech()->getLayer(lNum)->hasCutSpacing()) {
    return;
  }
  // obj1 = curr obj
  // obj2 = other obj
  // default via dimension
  frViaDef* viaDef = getDesign()->getTech()->getLayer(lNum)->getDefaultViaDef();
  frVia via(viaDef);
  frBox viaBox(0,0,0,0);
  via.getCutBBox(viaBox);

  bool isH = (getDir() == frPrefRoutingDirEnum::frcHorzPrefRoutingDir);
  frLayerNum followTrackLNum = (int)getDesign()->getTech()->getBottomLayerNum() - 1;
  if (lNum - 1 >= getDesign()->getTech()->getBottomLayerNum() && 
      getDesign()->getTech()->getLayer(lNum - 1)->getType() == frLayerTypeEnum::ROUTING &&
      getDesign()->getTech()->getLayer(lNum - 1)->getDir() == getDir()) {
    followTrackLNum = lNum - 1;
  } else if (lNum + 1 <= getDesign()->getTech()->getTopLayerNum() && 
             getDesign()->getTech()->getLayer(lNum + 1)->getType() == frLayerTypeEnum::ROUTING &&
             getDesign()->getTech()->getLayer(lNum + 1)->getDir() == getDir()) {
    followTrackLNum = lNum + 1;
  } else {
    cout <<"Warning: via layer connected to non-routing layer, skipped in modMinSpacingCostVia" <<endl;
    return;
  }

  // spacing value needed
  frCoord bloatDist = 0;
  for (auto con: getDesign()->getTech()->getLayer(lNum)->getCutSpacing()) {
    bloatDist = max(bloatDist, con->getCutSpacing());
  }

  int idx1, idx2;
  if (isH) {
    getTrackIdx(box.bottom() - bloatDist - (viaBox.top() - 0) + 1, 
                box.top()    + bloatDist + (0 - viaBox.bottom()) - 1,
                followTrackLNum, idx1, idx2);
  } else {
    getTrackIdx(box.left()   - bloatDist - (viaBox.right() - 0) + 1, 
                box.right()  + bloatDist + (0 - viaBox.left()) - 1,
                followTrackLNum, idx1, idx2);
  }

  auto &trackLocs = getTrackLocs(followTrackLNum);
  auto &workerRegionQuery = getWorkerRegionQuery();
  frBox tmpBx;
  frTransform xform;
  frCoord dx, dy, c2ctrackdist;
  //frCoord distSquare;
  frCoord reqDist = 0;
  frCoord maxX, blockLeft, blockRight;
  frBox blockBox;
  frPoint boxCenter, tmpBxCenter;
  boxCenter.set((box.left() + box.right()) / 2, (box.bottom() + box.top()) / 2);
  bool hasViol = false;
  for (int i = idx1; i <= idx2; i++) {
    auto trackLoc = trackLocs[i];
    //cout <<"@@@" <<trackLoc <<endl;
    if (isH) {
      xform.set(frPoint(box.left(), trackLoc));
    } else {
      xform.set(frPoint(trackLoc, box.bottom()));
    }
    tmpBx.set(viaBox);
    tmpBx.transform(xform);
    //distSquare = box2boxDistSquare(box, tmpBx, dx, dy);
    box2boxDistSquare(box, tmpBx, dx, dy);

    for (auto con: getDesign()->getTech()->getLayer(lNum)->getCutSpacing()) {
      hasViol       = false;
      reqDist       = con->getCutSpacing();
      bool isC2C = con->hasCenterToCenter();
      if (isH) {
        c2ctrackdist = abs(boxCenter.y() - trackLoc);
      } else {
        c2ctrackdist = abs(boxCenter.x() - trackLoc);
      }

      if (isH) {
        if (isC2C) {
          if (c2ctrackdist >= reqDist) {
            continue;
          }
        } else {
          if (dy >= reqDist) {
            continue;
          }
        }
        //cout <<"@@@@" <<trackLoc <<endl;
        if (isC2C) {
          maxX = (frCoord)(sqrt(1.0 * reqDist * reqDist - 1.0 * c2ctrackdist * c2ctrackdist));
        } else {
          maxX = (frCoord)(sqrt(1.0 * reqDist * reqDist - 1.0 * dy * dy));
        }
        if (maxX * maxX + dy * dy == reqDist * reqDist) {
          maxX = max(0, maxX - 1);
        }
        if (isC2C) {
          blockLeft  = boxCenter.x() - maxX;
          blockRight = boxCenter.x() + maxX;
        } else {
          blockLeft  = box.left()  - maxX - (viaBox.right() - 0);
          blockRight = box.right() + maxX + (0- viaBox.left());
        }
        blockBox.set(blockLeft, trackLoc, blockRight, trackLoc);
      } else {
        if (isC2C) {
          if (c2ctrackdist >= reqDist) {
            continue;
          }
        } else {
          if (dx >= reqDist) {
            continue;
          }
        }
        //cout <<"@@@@" <<trackLoc <<endl;
        if (isC2C) {
          maxX = (frCoord)(sqrt(1.0 * reqDist * reqDist - 1.0 * c2ctrackdist * c2ctrackdist));
        } else {
          maxX = (frCoord)(sqrt(1.0 * reqDist * reqDist - 1.0 * dx * dx));
        }
        if (maxX * maxX + dx * dx == reqDist * reqDist) {
          maxX = max(0, maxX - 1);
        }
        if (isC2C) {
          blockLeft  = boxCenter.y() - maxX;
          blockRight = boxCenter.y() + maxX;
        } else {
          blockLeft  = box.bottom()  - maxX - (viaBox.top() - 0);
          blockRight = box.top()     + maxX + (0- viaBox.bottom());
        }

        blockBox.set(trackLoc, blockLeft, trackLoc, blockRight);
      }
      if (con->hasSameNet()) {
        continue;
      }
      if (con->isLayer()) {
        ;
      } else if (con->isAdjacentCuts()) {
        hasViol = true;
        // should disable hasViol and modify this part to new grid graph
      } else if (con->isParallelOverlap()) {
        if (isH) {
          if (dy > 0) {
            blockBox.set(max(box.left()  - (viaBox.right() - 0) + 1, blockLeft), trackLoc, 
                         min(box.right() + (0 - viaBox.left())  - 1, blockRight), trackLoc);
          }
        } else {
          if (dx > 0) {
            blockBox.set(trackLoc, max(box.bottom() - (viaBox.top() - 0)     + 1, blockLeft), 
                         trackLoc, min(box.top()    + (0 - viaBox.bottom())  - 1, blockRight));
          }
        }
        if (blockBox.left() <= blockBox.right() && blockBox.bottom() <= blockBox.top()) {
          hasViol = true;
        }
      } else if (con->isArea()) {
        auto currArea = max(box.length() * box.width(), tmpBx.length() * tmpBx.width());
        if (currArea >= con->getCutArea()) {
          hasViol = true;
        }
      } else {
        hasViol = true;
      }
      if (hasViol) {
        if (isAddCost) {
          workerRegionQuery.addCost(blockBox, lNum, fig, con);
          if (pinS) {
            workerRegionQuery.query(blockBox, lNum, *pinS);
          }
          if (enableOutput) {
            cout <<"  add cutSpc via@("
                 <<blockBox.left()  / dbu <<", " <<blockBox.bottom() / dbu <<") (" 
                 <<blockBox.right() / dbu <<", " <<blockBox.top()    / dbu <<") " 
                 <<getDesign()->getTech()->getLayer(lNum)->getName() <<endl <<flush;
          }
        } else {
          workerRegionQuery.removeCost(blockBox, lNum, fig, con);
          if (pinS) {
            workerRegionQuery.query(blockBox, lNum, *pinS);
          }
          if (enableOutput) {
            cout <<"  sub cutSpc via@(" 
                 <<blockBox.left()  / dbu <<", " <<blockBox.bottom() / dbu <<") (" 
                 <<blockBox.right() / dbu <<", " <<blockBox.top()    / dbu <<") " 
                 <<getDesign()->getTech()->getLayer(lNum)->getName() <<endl <<flush;
          }
        }
      }
    }
  }
}

void FlexTAWorker::addCost(taPinFig* fig, set<taPin*, frBlockObjectComp> *pinS) {
  modCost(fig, true, pinS);
}

void FlexTAWorker::subCost(taPinFig* fig, set<taPin*, frBlockObjectComp> *pinS) {
  modCost(fig, false, pinS);
}

void FlexTAWorker::modCost(taPinFig* fig, bool isAddCost, set<taPin*, frBlockObjectComp> *pinS) {
  if (fig->typeId() == tacPathSeg) {
    auto obj = static_cast<taPathSeg*>(fig);
    auto layerNum = obj->getLayerNum();
    frBox box;
    obj->getBBox(box);
    modMinSpacingCostPlanar(box, layerNum, obj, isAddCost, pinS); // must be current TA layer
    modMinSpacingCostVia(box, layerNum, obj, isAddCost, true, true, pinS);
    modMinSpacingCostVia(box, layerNum, obj, isAddCost, false, true, pinS);
  } else if (fig->typeId() == tacVia) {
    auto obj = static_cast<taVia*>(fig);
    frBox box;
    obj->getLayer1BBox(box); // assumes enclosure for via is always rectangle
    auto layerNum = obj->getViaDef()->getLayer1Num();
    // current TA layer
    if (getDir() == getDesign()->getTech()->getLayer(layerNum)->getDir()) {
      modMinSpacingCostPlanar(box, layerNum, obj, isAddCost, pinS);
    }
    modMinSpacingCostVia(box, layerNum, obj, isAddCost, true, false, pinS);
    modMinSpacingCostVia(box, layerNum, obj, isAddCost, false, false, pinS);

    obj->getLayer2BBox(box); // assumes enclosure for via is always rectangle
    layerNum = obj->getViaDef()->getLayer2Num();
    // current TA layer
    if (getDir() == getDesign()->getTech()->getLayer(layerNum)->getDir()) {
      modMinSpacingCostPlanar(box, layerNum, obj, isAddCost, pinS);
    }
    modMinSpacingCostVia(box, layerNum, obj, isAddCost, true, false, pinS);
    modMinSpacingCostVia(box, layerNum, obj, isAddCost, false, false, pinS);

    frTransform xform;
    frPoint pt;
    obj->getOrigin(pt);
    xform.set(pt);
    for (auto &uFig: obj->getViaDef()->getCutFigs()) {
      auto rect = static_cast<frRect*>(uFig.get());
      rect->getBBox(box);
      box.transform(xform);
      layerNum = obj->getViaDef()->getCutLayerNum();
      modCutSpacingCost(box, layerNum, obj, isAddCost, pinS);
    }
  } else {
    cout <<"Error: unsupported region query add" <<endl;
  }
}

void FlexTAWorker::assignIroute_availTracks(taPin* iroute, frLayerNum &lNum, int &idx1, int &idx2) {
  //bool enableOutput = true;
  bool enableOutput = false;
  lNum = iroute->getGuide()->getBeginLayerNum();
  frPoint gbp, gep, gIdx;
  frBox gBox;
  iroute->getGuide()->getPoints(gbp, gep);
  getDesign()->getTopBlock()->getGCellIdx(gbp, gIdx);
  getDesign()->getTopBlock()->getGCellBox(gIdx, gBox);
  bool isH = (getDir() == frPrefRoutingDirEnum::frcHorzPrefRoutingDir);
  frCoord coordLow  = isH ? gBox.bottom() : gBox.left();
  frCoord coordHigh = isH ? gBox.top()    : gBox.right();
  coordHigh--; // to avoid higher track == guide top/right
  getTrackIdx(coordLow, coordHigh, lNum, idx1, idx2);
  if (enableOutput) {
    double dbu = getDesign()->getTopBlock()->getDBUPerUU();
    cout <<" min/max track@" <<getTrackLocs(lNum)[idx1] / dbu;
    if (idx2 > idx1) {
      cout <<"/" <<getTrackLocs(lNum)[idx2] / dbu;
    }
    cout <<endl;
  }
}

frUInt4 FlexTAWorker::assignIroute_getWlenCost(taPin* iroute, frCoord trackLoc) {
  auto guide = iroute->getGuide();
  bool isH = (getDir() == frPrefRoutingDirEnum::frcHorzPrefRoutingDir);
  frPoint begin, end;
  guide->getPoints(begin, end);
  frBox endBox;
  frPoint idx;
  getDesign()->getTopBlock()->getGCellIdx(end, idx);
  getDesign()->getTopBlock()->getGCellBox(idx, endBox);
  int wlen = 0;
  auto wlen_helper = iroute->getWlenHelper();
  if (wlen_helper <= 0) {
    if (isH) {
      wlen = abs(wlen_helper) * (trackLoc - endBox.bottom());
    } else {
      wlen = abs(wlen_helper) * (trackLoc - endBox.left());
    }
  } else {
    if (isH) {
      wlen = abs(wlen_helper) * (endBox.top()   - trackLoc);
    } else {
      wlen = abs(wlen_helper) * (endBox.right() - trackLoc);
    }
  }
  if (wlen < 0) {
    double dbu = getDesign()->getTopBlock()->getDBUPerUU();
    cout <<"Error: getWlenCost has wlenCost < 0" <<", trackLoc@" <<trackLoc / dbu <<" box (" 
         <<endBox.left()  / dbu  <<", " <<endBox.bottom() / dbu <<") (" 
         <<endBox.right() / dbu  <<", " <<endBox.top()    / dbu <<")" <<endl;
    return (frUInt4)0;
  } else {
    return (frUInt4)wlen;
  }
}

frUInt4 FlexTAWorker::assignIroute_getPinCost(taPin* iroute, frCoord trackLoc) {
  frUInt4 sol = 0;
  if (iroute->hasWlenHelper2()) {
    sol = abs(trackLoc - iroute->getWlenHelper2());
  }
  return sol;
}

frUInt4 FlexTAWorker::assignIroute_getDRCCost_helper(taPin* iroute, const frBox &box, frLayerNum lNum) {
  auto &workerRegionQuery = getWorkerRegionQuery();
  vector<rq_generic_value_t<std::pair<frBlockObject*, frConstraint*> > > result;
  int overlap = 0;
  workerRegionQuery.queryCost(box, lNum, result);
  bool isCut = false;
  for (auto &[boostb, pr]: result) {
    auto &[obj, con] = pr;
    frCoord tmpOvlp = - max(box.left(),   boostb.min_corner().x()) + min(box.right(),  boostb.max_corner().x()) 
                      - max(box.bottom(), boostb.min_corner().y()) + min(box.top(),    boostb.max_corner().y()) + 1;
    if (tmpOvlp <= 0) {
      cout <<"Error: assignIroute_getDRCCost_helper overlap < 0" <<endl;
      exit(1);
    }
    // unknown obj, always add cost
    if (obj == nullptr) {
      overlap += tmpOvlp;
    // only add cost for diff-net
    } else if (obj->typeId() == frcNet) {
      if (iroute->getGuide()->getNet() != obj) {
        overlap += tmpOvlp;
      }
    // two taObjs
    } else if (obj->typeId() == tacPathSeg || obj->typeId() == tacVia) {
      auto taObj = static_cast<taPinFig*>(obj);
      // can exclude same iroute objs also
      if (taObj->getPin() == iroute) {
        continue;
      }
      if (iroute->getGuide()->getNet() != taObj->getPin()->getGuide()->getNet()) {
        overlap += tmpOvlp;
      }
    } else {
      cout <<"Warning: assignIroute_getDRCCost_helper unsupported type" <<endl;
    }
  }
  frCoord pitch = 0;
  if (getDesign()->getTech()->getLayer(lNum)->getType() == frLayerTypeEnum::ROUTING) {
    pitch = getDesign()->getTech()->getLayer(lNum)->getPitch();
    isCut = false;
  } else if (getDesign()->getTech()->getTopLayerNum() >= lNum + 1 && getDesign()->getTech()->getLayer(lNum + 1)->getType() == frLayerTypeEnum::ROUTING) {
    pitch = getDesign()->getTech()->getLayer(lNum + 1)->getPitch();
    isCut = true;
  } else if (getDesign()->getTech()->getBottomLayerNum() <= lNum - 1 && getDesign()->getTech()->getLayer(lNum - 1)->getType() == frLayerTypeEnum::ROUTING) {
    pitch = getDesign()->getTech()->getLayer(lNum - 1)->getPitch();
    isCut = true;
  } else {
    cout <<"Error: assignIroute_getDRCCost_helper unknown layer type" <<endl;
    exit(1);
  }
  // always penalize two pitch per cut, regardless of cnts
  return (overlap == 0) ? 0 : (isCut ? pitch * 2: max(pitch * 2, overlap));
}

frUInt4 FlexTAWorker::assignIroute_getDRCCost(taPin* iroute, frCoord trackLoc) {
  frUInt4 cost = 0;
  frPoint bp, ep;
  bool isH = (getDir() == frPrefRoutingDirEnum::frcHorzPrefRoutingDir);
  for (auto &uPinFig: iroute->getFigs()) {
    if (uPinFig->typeId() == tacPathSeg) {
      auto obj = static_cast<taPathSeg*>(uPinFig.get());
      obj->getPoints(bp, ep);
      if (isH) {
        bp.set(bp.x(), trackLoc);
        ep.set(ep.x(), trackLoc);
      } else {
        bp.set(trackLoc, bp.y());
        ep.set(trackLoc, ep.y());
      }
      frUInt4 wireCost = assignIroute_getDRCCost_helper(iroute, frBox(bp, ep), obj->getLayerNum());
      //if (!isInitTA()) {
      //  cout <<"wireCost@" <<wireCost <<endl;
      //}
      cost += wireCost;
    } else if (uPinFig->typeId() == tacVia) {
      auto obj = static_cast<taVia*>(uPinFig.get());
      obj->getOrigin(bp);
      if (isH) {
        bp.set(bp.x(), trackLoc);
      } else {
        bp.set(trackLoc, bp.y());
      }
      //cost += TAVIACOST * assignIroute_getDRCCost_helper(iroute, frBox(bp, bp), obj->getViaDef()->getCutLayerNum());
      frUInt4 viaCost = assignIroute_getDRCCost_helper(iroute, frBox(bp, bp), obj->getViaDef()->getCutLayerNum());
      //if (!isInitTA()) {
      //  cout <<"viaCost@" <<viaCost <<endl;
      //}
      cost += viaCost;
    } else {
      cout <<"Error: assignIroute_updateIroute unsupported pinFig" <<endl;
      exit(1);
    }
  }
  return cost;
}

frUInt4 FlexTAWorker::assignIroute_getAlignCost(taPin* iroute, frCoord trackLoc) {
  frUInt4 sol = 0;
  frCoord pitch = 0;
  bool isH = (getDir() == frPrefRoutingDirEnum::frcHorzPrefRoutingDir);
  for (auto &uPinFig: iroute->getFigs()) {
    if (uPinFig->typeId() == tacPathSeg) {
      auto obj=  static_cast<taPathSeg*>(uPinFig.get());
      frPoint bp, ep;
      obj->getPoints(bp, ep);
      auto lNum = obj->getLayerNum();
      pitch = getDesign()->getTech()->getLayer(lNum)->getPitch();
      auto &workerRegionQuery = getWorkerRegionQuery();
      set<taPin*, frBlockObjectComp> result;
      frBox box;
      if (isH) {
        box.set(bp.x(), trackLoc, ep.x(), trackLoc);
      } else {
        box.set(trackLoc, bp.y(), trackLoc, ep.y());
      }
      workerRegionQuery.query(box, lNum, result);
      for (auto &iroute2: result) {
        if (iroute2->getGuide()->getNet() == iroute->getGuide()->getNet()) {
          sol = 1;
          break;
        }
      }
    }
    if (sol == 1) {
      break;
    }
  }
  return pitch * sol;
}

frUInt4 FlexTAWorker::assignIroute_getCost(taPin* iroute, frCoord trackLoc, frUInt4 &outDrcCost) {
  //bool enableOutput = true;
  bool enableOutput = false;
  outDrcCost     = assignIroute_getDRCCost(iroute, trackLoc);
  int drcCost    = TADRCCOST * outDrcCost;
  int wlenCost   = assignIroute_getWlenCost(iroute, trackLoc);
  int pinCost    = TAPINCOST * assignIroute_getPinCost(iroute, trackLoc);
  int alignCost  = TAALIGNCOST * assignIroute_getAlignCost(iroute, trackLoc);
  if (enableOutput) {
    cout <<"    drc/wlen/pin/align cost = " <<drcCost <<"/" <<wlenCost <<"/" <<pinCost <<"/" <<alignCost <<endl;
  }
  return max(drcCost + wlenCost + pinCost - alignCost, 0);
}

void FlexTAWorker::assignIroute_bestTrack_helper(taPin* iroute, frLayerNum lNum, int trackIdx, frUInt4 &bestCost, 
                                                 frCoord &bestTrackLoc, int &bestTrackIdx, frUInt4 &drcCost) {
  //bool enableOutput = true;
  bool enableOutput = false;
  double dbu = getDesign()->getTopBlock()->getDBUPerUU();
  auto trackLoc = getTrackLocs(lNum)[trackIdx];
  auto currCost = assignIroute_getCost(iroute, trackLoc, drcCost);
  if (isInitTA()) {
    if (currCost < bestCost) {
      bestCost = currCost;
      bestTrackLoc = trackLoc;
      bestTrackIdx = trackIdx;
    }
  } else {
    if (drcCost < bestCost) {
      bestCost = drcCost;
      bestTrackLoc = trackLoc;
      bestTrackIdx = trackIdx;
    }
  }
  if (enableOutput) {
    cout <<"  try track@" <<trackLoc / dbu <<", cost/drc=" <<currCost <<"/" <<drcCost <<endl;
  }
}

int FlexTAWorker::assignIroute_bestTrack(taPin* iroute, frLayerNum lNum, int idx1, int idx2) {
  //bool enableOutput = true;
  bool enableOutput = false;
  double  dbu = getDesign()->getTopBlock()->getDBUPerUU();
  frCoord bestTrackLoc = 0;
  int     bestTrackIdx = -1;
  frUInt4 bestCost = std::numeric_limits<frUInt4>::max();
  frUInt4 drcCost = 0;
  //while (1) {
  // if wlen2, then try from  wlen2
  // else try from wlen1 dir
  if (iroute->hasWlenHelper2()) {
    frCoord wlen2coord = iroute->getWlenHelper2();
    if (iroute->getWlenHelper() > 0) {
      if (enableOutput) {
        cout <<" use wlen2@" <<wlen2coord / dbu <<", wlen@" <<iroute->getWlenHelper() <<endl;
      }
      int startTrackIdx = int(std::lower_bound(trackLocs[lNum].begin(), trackLocs[lNum].end(), wlen2coord) - trackLocs[lNum].begin());
      startTrackIdx = min(startTrackIdx, idx2);
      startTrackIdx = max(startTrackIdx, idx1);
      for (int i = startTrackIdx; i <= idx2; i++) {
        assignIroute_bestTrack_helper(iroute, lNum, i, bestCost, bestTrackLoc, bestTrackIdx, drcCost);
        if (!drcCost) {
          break;
        }
      }
      if (drcCost) {
        for (int i = startTrackIdx - 1; i >= idx1; i--) {
          assignIroute_bestTrack_helper(iroute, lNum, i, bestCost, bestTrackLoc, bestTrackIdx, drcCost);
          if (!drcCost) {
            break;
          }
        }
      }
    } else if (iroute->getWlenHelper() == 0) {
      if (enableOutput) {
        cout <<" use wlen2@" <<wlen2coord / dbu <<", wlen@" <<iroute->getWlenHelper() <<endl;
      }
      int startTrackIdx = int(std::lower_bound(trackLocs[lNum].begin(), trackLocs[lNum].end(), wlen2coord) - trackLocs[lNum].begin());
      startTrackIdx = min(startTrackIdx, idx2);
      startTrackIdx = max(startTrackIdx, idx1);
      for (int i = 0; i < idx2 - idx1; i++) {
        int currTrackIdx = startTrackIdx + i;
        if (currTrackIdx >= idx1 && currTrackIdx <= idx2) {
          assignIroute_bestTrack_helper(iroute, lNum, currTrackIdx, bestCost, bestTrackLoc, bestTrackIdx, drcCost);
        }
        if (!drcCost) {
          break;
        }
        currTrackIdx = startTrackIdx - i - 1;
        if (currTrackIdx >= idx1 && currTrackIdx <= idx2) {
          assignIroute_bestTrack_helper(iroute, lNum, currTrackIdx, bestCost, bestTrackLoc, bestTrackIdx, drcCost);
        }
        if (!drcCost) {
          break;
        }
      }
    } else {
      if (enableOutput) {
        cout <<" use wlen2@" <<wlen2coord / dbu <<", wlen@" <<iroute->getWlenHelper() <<endl;
      }
      int startTrackIdx = int(std::lower_bound(trackLocs[lNum].begin(), trackLocs[lNum].end(), wlen2coord) - trackLocs[lNum].begin());
      startTrackIdx = min(startTrackIdx, idx2);
      startTrackIdx = max(startTrackIdx, idx1);
      for (int i = startTrackIdx; i >= idx1; i--) {
        assignIroute_bestTrack_helper(iroute, lNum, i, bestCost, bestTrackLoc, bestTrackIdx, drcCost);
        if (!drcCost) {
          break;
        }
      }
      if (drcCost) {
        for (int i = startTrackIdx + 1; i <= idx2; i++) {
          assignIroute_bestTrack_helper(iroute, lNum, i, bestCost, bestTrackLoc, bestTrackIdx, drcCost);
          if (!drcCost) {
            break;
          }
        }
      }
    }
  } else {
    if (iroute->getWlenHelper() > 0) {
      if (enableOutput) {
        cout <<" use wlen@" <<iroute->getWlenHelper() <<endl;
      }
      for (int i = idx2; i >= idx1; i--) {
        assignIroute_bestTrack_helper(iroute, lNum, i, bestCost, bestTrackLoc, bestTrackIdx, drcCost);
        if (!drcCost) {
          break;
        }
      }
    } else if (iroute->getWlenHelper() == 0) {
      if (enableOutput) {
        cout <<" use wlen@" <<iroute->getWlenHelper() <<endl;
      }
      for (int i = (idx1 + idx2) / 2; i <= idx2; i++) {
        assignIroute_bestTrack_helper(iroute, lNum, i, bestCost, bestTrackLoc, bestTrackIdx, drcCost);
        if (!drcCost) {
          break;
        }
      }
      if (drcCost) {
        for (int i = (idx1 + idx2) / 2 - 1; i >= idx1; i--) {
          assignIroute_bestTrack_helper(iroute, lNum, i, bestCost, bestTrackLoc, bestTrackIdx, drcCost);
          if (!drcCost) {
            break;
          }
        }
      }
    } else {
      if (enableOutput) {
        cout <<" use wlen@" <<iroute->getWlenHelper() <<endl;
      }
      for (int i = idx1; i <= idx2; i++) {
        assignIroute_bestTrack_helper(iroute, lNum, i, bestCost, bestTrackLoc, bestTrackIdx, drcCost);
        if (!drcCost) {
          break;
        }
      }
    }
  }
  if (bestTrackIdx == -1) {
    cout <<"Error: assignIroute_bestTrack select no track" <<endl;
    exit(1);
  }
  if (enableOutput) {
  //if (true) {
    cout <<"  select track@" <<bestTrackLoc / dbu <<", cost=" <<bestCost <<endl;
  }
  //totCost    -= iroute->getCost();
  //totDrcCost -= iroute->getDrcCost();
  totCost    += drcCost;
  iroute->setCost(drcCost);
  //totDrcCost += drcCost;
  return bestTrackLoc;
}
  
void FlexTAWorker::assignIroute_updateIroute(taPin* iroute, frCoord bestTrackLoc, set<taPin*, frBlockObjectComp> *pinS) {
  auto &workerRegionQuery = getWorkerRegionQuery();
  bool isH = (getDir() == frPrefRoutingDirEnum::frcHorzPrefRoutingDir);
  frPoint bp, ep;

  // update coord
  for (auto &uPinFig: iroute->getFigs()) {
    if (uPinFig->typeId() == tacPathSeg) {
      auto obj = static_cast<taPathSeg*>(uPinFig.get());
      obj->getPoints(bp, ep);
      if (isH) {
        bp.set(bp.x(), bestTrackLoc);
        ep.set(ep.x(), bestTrackLoc);
      } else {
        bp.set(bestTrackLoc, bp.y());
        ep.set(bestTrackLoc, ep.y());
      }
      obj->setPoints(bp, ep);
    } else if (uPinFig->typeId() == tacVia) {
      auto obj = static_cast<taVia*>(uPinFig.get());
      obj->getOrigin(bp);
      if (isH) {
        bp.set(bp.x(), bestTrackLoc);
      } else {
        bp.set(bestTrackLoc, bp.y());
      }
      obj->setOrigin(bp);
    } else {
      cout <<"Error: assignIroute_updateIroute unsupported pinFig" <<endl;
      exit(1);
    }
  }
  // addCost
  for (auto &uPinFig: iroute->getFigs()) {
    addCost(uPinFig.get(), isInitTA() ? nullptr : pinS);
    workerRegionQuery.add(uPinFig.get());
  }
  iroute->addNumAssigned();
}

void FlexTAWorker::assignIroute_init(taPin* iroute, set<taPin*, frBlockObjectComp> *pinS) {
  auto &workerRegionQuery = getWorkerRegionQuery();
  // subCost
  if (!isInitTA()) {
    for (auto &uPinFig: iroute->getFigs()) {
      workerRegionQuery.remove(uPinFig.get());
      subCost(uPinFig.get(), pinS);
    }
    totCost    -= iroute->getCost();
    //totDrcCost -= iroute->getDrcCost();
  }
}

void FlexTAWorker::assignIroute_updateOthers(set<taPin*, frBlockObjectComp> &pinS) {
  //bool enableOutput = true;
  bool enableOutput = false;
  bool isH = (getDir() == frPrefRoutingDirEnum::frcHorzPrefRoutingDir);
  frPoint bp, ep;
  if (isInitTA()) {
    return;
  }
  for (auto &iroute: pinS) {
    removeFromReassignIroutes(iroute);
    // recalculate cost
    frUInt4 drcCost = 0;
    frCoord trackLoc = std::numeric_limits<frCoord>::max();
    for (auto &uPinFig: iroute->getFigs()) {
      if (uPinFig->typeId() == tacPathSeg) {
        static_cast<taPathSeg*>(uPinFig.get())->getPoints(bp, ep);
        if (isH) {
          trackLoc = bp.y();
        } else {
          trackLoc = bp.x();
        }
        break;
      }
    }
    if (trackLoc == std::numeric_limits<frCoord>::max()) {
      cout <<"Error: FlexTAWorker::assignIroute_updateOthers does not find trackLoc" <<endl;
      exit(1);
    }
    totCost    -= iroute->getCost();
    //totDrcCost -= iroute->getDrcCost();
    //auto tmpCost = assignIroute_getCost(iroute, trackLoc, drcCost);
    assignIroute_getCost(iroute, trackLoc, drcCost);
    iroute->setCost(drcCost);
    //iroute->setCost(tmpCost);
    //iroute->setDrcCost(drcCost);
    //totCost    += iroute->getCost();
    //totDrcCost += iroute->getDrcCost();
    totCost    += iroute->getCost();
    if (drcCost && iroute->getNumAssigned() < maxRetry) {
      addToReassignIroutes(iroute);
    }
  }
  if (enableOutput && pinS.size()) {
    cout <<"updated " <<pinS.size() <<" iroutes" <<endl;
  }
}

void FlexTAWorker::assignIroute(taPin* iroute) {
  //bool enableOutput = true;
  bool enableOutput = false;
  if (enableOutput) {
    bool   isH = (getDir() == frPrefRoutingDirEnum::frcHorzPrefRoutingDir);
    double dbu = getDesign()->getTopBlock()->getDBUPerUU();
    frPoint bp, ep;
    frCoord bc, ec, trackLoc;
    cout <<"assigning " <<iroute->getId() <<" " <<iroute->getGuide()->getNet()->getName();
    auto guideLNum = iroute->getGuide()->getBeginLayerNum();
    for (auto &uPinFig: iroute->getFigs()) {
      if (uPinFig->typeId() == tacPathSeg) {
        auto obj = static_cast<taPathSeg*>(uPinFig.get());
        obj->getPoints(bp, ep);
        bc = isH ? bp.x() : bp.y();
        ec = isH ? ep.x() : ep.y();
        trackLoc = isH ? bp.y() : bp.x();
        cout <<" (" <<bc / dbu <<"-->" <<ec / dbu <<"), len@" <<(ec - bc) / dbu <<", track@" <<trackLoc / dbu
             <<", " <<getDesign()->getTech()->getLayer(iroute->getGuide()->getBeginLayerNum())->getName();
      } else if (uPinFig->typeId() == tacVia) {
        auto obj = static_cast<taVia*>(uPinFig.get());
        auto cutLNum = obj->getViaDef()->getCutLayerNum();
        obj->getOrigin(bp);
        bc = isH ? bp.x() : bp.y();
        cout <<string((cutLNum > guideLNum) ? ", U@" : ", D@") <<bc / dbu;
      }
    }
    cout <<", wlen_h@" <<iroute->getWlenHelper() <<", cost@" <<iroute->getCost() <<endl;
  }

  set<taPin*, frBlockObjectComp> pinS;
  assignIroute_init(iroute, &pinS);
  frLayerNum lNum;
  int idx1, idx2;
  assignIroute_availTracks(iroute, lNum, idx1, idx2);
  auto bestTrackLoc = assignIroute_bestTrack(iroute, lNum, idx1, idx2);

  assignIroute_updateIroute(iroute, bestTrackLoc, &pinS);
  assignIroute_updateOthers(pinS);
}

void FlexTAWorker::assign() {
  //bool enableOutput = true;
  bool enableOutput = false;
  if (getTAIter() == -1) {
    return;
  }
  //if (isInitTA()) {
    int maxBufferSize = 20;
    vector<taPin*> buffers(maxBufferSize, nullptr);
    int currBufferIdx = 0;
    auto iroute = popFromReassignIroutes();
    while (iroute != nullptr) {
      auto it = find(buffers.begin(), buffers.end(), iroute);
      // in the buffer, skip
      if (it != buffers.end() || iroute->getNumAssigned() >= maxRetry) {
        ;
      // not in the buffer, re-assign
      } else {
        assignIroute(iroute);
        // re add last buffer item to reassigniroutes if drccost > 0
        //if (buffers[currBufferIdx]) {
        //  if (buffers[currBufferIdx]->getDrcCost()) {
        //    addToReassignIroutes(buffers[currBufferIdx]);
        //  }
        //}
        buffers[currBufferIdx] = iroute;
        currBufferIdx = (currBufferIdx + 1) % maxBufferSize;
        if (enableOutput && !isInitTA()) {
          //cout <<"totCost@" <<totCost <<"/" <<totDrcCost <<endl;
          cout <<"totCost@" <<totCost <<endl;
        }
        numAssigned++;
      }
      iroute = popFromReassignIroutes();
    }
  //}
}

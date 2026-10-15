#include "ServingSizePolicy.h"
#include "Catalog.h"
#include <iostream>
#include <limits>
int checks=0;
void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv) {
    try {
        using namespace inn::serving;
        Settings disabled;Settings compact{true,14};
        Check(!disabled.compactMain,"Optional feature enabled by default");
        Check(MainScale(40,30,true,3,disabled)==1,"Disabled mode changed food size");
        Check(MainScale(6.6772f,6.8393f,false,.8f,compact)==1,"Ordinary soup was shrunk");
        Check(MainScale(3,4,false,1,compact)==1,"Small food was enlarged or shrunk");
        Check(MainScale(20,10,false,1,compact)<1,"Oversized food was not reduced");
        Check(!Valid({true,std::numeric_limits<float>::quiet_NaN()})&&!Valid({true,0}),"Invalid target accepted");
        Check(MainScale(20,10,true,0,compact)==1,"Invalid plate scale produced a broken serving");
        Check(!disabled.compactDesserts,"Dessert option enabled by default");
        Settings sidesOnly{false,14,true,14};
        Check(CourseScale(0,20,10,true,2,sidesOnly)==1,"Dessert toggle changed mains");
        Check(CourseScale(2,20,10,true,2,compact)==1,"Main toggle changed desserts");
        Check(CourseScale(2,20,10,true,2,sidesOnly)<1,"Dessert-only toggle did not shrink food");
        Check(CourseScale(2,20,10,true,8,sidesOnly)==CourseScale(2,20,10,false,1,sidesOnly),"Dessert incorrectly inherited the main plate footprint");
        Check(CourseScale(1,20,10,true,2,{true,14,true,14})==1,"Drink was shrunk");
        Check(CourseScale(2,6.6772f,6.8393f,false,1,sidesOnly)==1,"Soup smaller than target was shrunk");
        Check(CourseScale(-1,20,10,false,1,sidesOnly)==1&&CourseScale(3,20,10,false,1,sidesOnly)==1,"Unknown course was scaled");
        Settings different{true,22,true,14};
        Check(CourseScale(0,20,10,false,1,different)>CourseScale(2,20,10,false,1,different),"Independent target sizes were not honored");
        Check(argc==2,"Missing catalog");inn::Catalog catalog;catalog.Load(argv[1]);
        int affected=0,affectedSides=0;bool cheese=false;
        for(const auto& food:catalog.foods)if(food.mask&1) {
            for(float target:{14.f,18.f,22.f,40.f}) {
                Settings value{true,target};const float scale=MainScale(food.width,food.depth,food.plate,food.scale,value);
                Check(scale>0&&scale<=1&&std::isfinite(scale),"Food scale is not a finite reduction");
                Check(food.width*scale<=target*.5f+.001f&&food.depth*scale<=target*.5f+.001f,"Actual food footprint exceeds the target");
                if(food.plate)Check(plateHalfWidth*food.scale*scale+.5f<=target*.5f+.001f,"Plate remains oversized after shrinking food");
                Check(MainScale(food.width,food.depth,food.plate,food.scale,disabled)==1,"Off mode changed a catalog entry");
            }
            if(MainScale(food.width,food.depth,food.plate,food.scale,compact)<1)++affected;
        }
        Check(affected>20,"Oversized main coverage unexpectedly small");
        for(const auto& food:catalog.foods)if(food.mask&4) {
            for(float target:{14.f,18.f,22.f,40.f}) {
                auto value=sidesOnly;value.targetDessertDiameter=target;
                const auto scale=CourseScale(2,food.width,food.depth,food.plate,food.scale,value);
                Check(scale>0&&scale<=1&&std::isfinite(scale),"Invalid dessert scale");
                Check(food.width*scale<=target*.5f+.001f&&food.depth*scale<=target*.5f+.001f,"Dessert footprint exceeds own target");
                Check(CourseScale(2,food.width,food.depth,food.plate,food.scale,compact)==1,"Main-only mode shrank a side profile");
                Check(CourseScale(0,food.width,food.depth,food.plate,food.scale,value)==1,"Side-only mode shrank a main profile");
            }
            if(CourseScale(2,food.width,food.depth,food.plate,food.scale,sidesOnly)<1)++affectedSides;
            if(food.plugin=="skyrim.esm"&&food.local==0x64B31) {
                cheese=true;Check(CourseScale(2,food.width,food.depth,food.plate,food.scale,sidesOnly)<1,"Large goat cheese wedge not included");
            }
        }
        Check(affectedSides>0,"No large side profiles shrink");Check(cheese,"Cheese fixture missing");
        std::cout<<checks<<" serving size checks passed; "<<affected<<" main and "<<affectedSides<<" dessert/side profiles compacted at soup-size target\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<'\n';return 1;}
}

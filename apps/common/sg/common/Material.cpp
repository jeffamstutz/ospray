// ======================================================================== //
// Copyright 2009-2016 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#include "sg/common/Material.h"
#include "sg/common/World.h"
#include "sg/common/Integrator.h"
#include "ospray/ospray.h"

namespace ospray {
  namespace sg {

    /*! constructor */
    Material::Material()
      : ospMaterial(NULL),
        name(""),
        type("")
    {
      add(createNode("type", "string", std::string("default")));
      vec3f kd(.6f);
      vec3f ks(.8f);
      add(createNode("Kd", "vec3f",kd, NodeFlags::required | NodeFlags::valid_min_max));
      getChild("Kd")->setMinMax(vec3f(0), vec3f(1));
      add(createNode("Ks", "vec3f",ks, NodeFlags::required | NodeFlags::valid_min_max));
      getChild("Ks")->setMinMax(vec3f(0), vec3f(1));
      add(createNode("Ns", "float",4.f, NodeFlags::required | NodeFlags::valid_min_max));
      getChild("Ns")->setMinMax(0.f, 100.f);
    }

    void Material::preCommit(RenderContext &ctx)
    {
      if (ospMaterial != nullptr) return;
      OSPMaterial mat = ospNewMaterial(ctx.ospRenderer, type.c_str());
      if (!mat)
      {
          std::cerr << "Warning: Could not create material type '" << type << "'. Replacing with default material." << std::endl;
          static OSPMaterial defaultMaterial = NULL;
          if (!defaultMaterial) {
            defaultMaterial = ospNewMaterial(ctx.integrator->getOSPHandle(), "default");
            vec3f kd(.7f);
            vec3f ks(.3f);
            ospSet3fv(defaultMaterial, "Kd", &kd.x);
            ospSet3fv(defaultMaterial, "Ks", &ks.x);
            ospSet1f(defaultMaterial, "Ns", 99.f);
            ospCommit(defaultMaterial);
          }
          mat = defaultMaterial;
        }
      setValue((OSPObject)mat);
      ospMaterial = mat;
    }

    void Material::postCommit(RenderContext &ctx)
    {
      ospCommit(ospMaterial);
    }

    void Material::render(RenderContext &ctx)
    {
      if (ospMaterial) return;

      PING;
      PRINT(ctx.integrator->toString());
      ospMaterial = ospNewMaterial(ctx.integrator->getOSPHandle(), type.c_str());
      setValue((OSPObject)ospMaterial);

      //We failed to create a material of the given type, handle it
      if (!ospMaterial) {
        std::cerr << "Warning: Could not create material type '" << type << "'. Replacing with default material." << std::endl;
        //Replace with default
        static OSPMaterial defaultMaterial = NULL;
        if (!defaultMaterial) {
          defaultMaterial = ospNewMaterial(ctx.integrator->getOSPHandle(), "default");
          vec3f kd(.7f);
          vec3f ks(.3f);
          ospSet3fv(defaultMaterial, "Kd", &kd.x);
          ospSet3fv(defaultMaterial, "Ks", &ks.x);
          ospSet1f(defaultMaterial, "Ns", 99.f);
          ospCommit(defaultMaterial);
        }
        ospMaterial = defaultMaterial;
        return;
      }

      for(size_t i = 0; i < textures.size(); i++) {
        textures[i]->render(ctx);
      }
      
      //Forward all params on to the ospMaterial...
      for_each_param([&](const std::shared_ptr<Param> &param){
          switch(param->getOSPDataType()) {
          case OSP_INT:
          case OSP_UINT:
            {
              ParamT<int> *p = (ParamT<int>*)param.get();
              if(param->getName().find("map_") != std::string::npos) {
                //Handle textures!
                assert(textures[p->value]->ospTexture != NULL && "Texture should not be null at this point.");
                ospSetObject(ospMaterial, param->getName().c_str(), textures[p->value]->ospTexture);
              } else {
                ospSet1i(ospMaterial, param->getName().c_str(), p->value);
              }
            }
            break;
          case OSP_INT3:
          case OSP_UINT3:
            {
              ParamT<vec3i> *p = (ParamT<vec3i>*)param.get();
              ospSet3i(ospMaterial, param->getName().c_str(), p->value.x, p->value.y, p->value.z);
            }
            break;
          case OSP_FLOAT:
            {
              ParamT<float> *p = (ParamT<float>*)param.get();
              ospSet1f(ospMaterial, param->getName().c_str(), p->value);
            }
            break;
          case OSP_FLOAT2:
            {
              ParamT<vec2f> *p = (ParamT<vec2f>*)param.get();
              ospSet2fv(ospMaterial, param->getName().c_str(), &p->value.x);
            }
            break;
          case OSP_FLOAT3:
            {
              ParamT<vec3f> *p = (ParamT<vec3f>*)param.get();
              ospSet3fv(ospMaterial, param->getName().c_str(), &p->value.x);
            }
            break;
          case OSP_TEXTURE:
            {
              ParamT<Ref<Texture2D>> *p = (ParamT<Ref<Texture2D>>*)param.get();
              Texture2D *tex = p->value.ptr;
              if (tex) {
                tex->render(ctx);
                if (tex->ospTexture) {
                  std::cout << "setting texture " << p->value->toString() << " to mat value " << param->getName() << std::endl;
                  ospSetObject(ospMaterial, param->getName().c_str(), p->value->ospTexture);
                }
              }
            }
            break;
          default: //Catch not yet implemented data types
            PRINT(param->getOSPDataType());
            std::cerr << "Warning: parameter '" << param->getName() << "' of material '" << name << "' had an invalid data type and will be ignored." << std::endl;
          }
        });

      ospCommit(ospMaterial);
    }

    OSP_REGISTER_SG_NODE(Material);

  }
}

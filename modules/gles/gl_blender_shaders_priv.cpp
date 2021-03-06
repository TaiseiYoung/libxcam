/*
 * gl_blender_shaders_priv.cpp - gl blender shaders private class implementation
 *
 *  Copyright (c) 2018 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Author: Yinhang Liu <yinhangx.liu@intel.com>
 */

#include "gl_blender_shaders_priv.h"

namespace XCam {

namespace XCamGLShaders {

enum {
    ShaderGaussScalePyr = 0,
    ShaderLapTransPyr,
    ShaderBlendPyr,
    ShaderReconstructPyr
};

static const GLShaderInfo shaders_info[] = {
    {
        GL_COMPUTE_SHADER,
        "shader_gauss_scale_pyr",
#include "shader_gauss_scale_pyr.comp.slx"
        , 0
    },
    {
        GL_COMPUTE_SHADER,
        "shader_lap_trans_pyr",
#include "shader_lap_trans_pyr.comp.slx"
        , 0
    },
    {
        GL_COMPUTE_SHADER,
        "shader_blend_pyr",
#include "shader_blend_pyr.comp.slx"
        , 0
    },
    {
        GL_COMPUTE_SHADER,
        "shader_reconstruct_pyr",
#include "shader_reconstruct_pyr.comp.slx"
        , 0
    }
};

bool
GLGaussScalePyrShader::check_desc (
    const GLBufferDesc &in_desc, const GLBufferDesc &out_desc, const Rect &merge_area)
{
    XCAM_FAIL_RETURN (
        ERROR,
        merge_area.pos_y == 0 && merge_area.height == (int32_t)in_desc.height &&
        merge_area.pos_x + merge_area.width <= (int32_t)in_desc.width &&
        merge_area.width == (int32_t)out_desc.width * 2 &&
        merge_area.height == (int32_t)out_desc.height * 2,
        false, "invalid buffer size: input:%dx%d, output:%dx%d, merge_area:%dx%d",
        in_desc.width, in_desc.height, out_desc.width, out_desc.height, merge_area.width, merge_area.height);

    return true;
}

XCamReturn
GLGaussScalePyrShader::prepare_arguments (const SmartPtr<Worker::Arguments> &base, GLCmdList &cmds)
{
    SmartPtr<GLGaussScalePyrShader::Args> args = base.dynamic_cast_ptr<GLGaussScalePyrShader::Args> ();
    XCAM_ASSERT (args.ptr () && args->in_glbuf.ptr () && args->out_glbuf.ptr ());

    const GLBufferDesc &in_desc = args->in_glbuf->get_buffer_desc ();
    const GLBufferDesc &out_desc = args->out_glbuf->get_buffer_desc ();
    const Rect &merge_area = args->merge_area;
    XCAM_FAIL_RETURN (
        ERROR, check_desc (in_desc, out_desc, merge_area), XCAM_RETURN_ERROR_PARAM,
        "GLGaussScalePyrShader(%s) check buffer description failed, level:%d idx:%d",
        XCAM_STR (get_name ()), args->level, (int)args->idx);

    cmds.push_back (new GLCmdBindBufRange (args->in_glbuf, 0, NV12PlaneYIdx));
    cmds.push_back (new GLCmdBindBufRange (args->in_glbuf, 1, NV12PlaneUVIdx));
    cmds.push_back (new GLCmdBindBufRange (args->out_glbuf, 2, NV12PlaneYIdx));
    cmds.push_back (new GLCmdBindBufRange (args->out_glbuf, 3, NV12PlaneUVIdx));

    size_t unit_bytes = sizeof (uint32_t);
    uint32_t in_img_width = XCAM_ALIGN_UP (in_desc.width, unit_bytes) / unit_bytes;
    uint32_t in_offset_x = XCAM_ALIGN_UP (merge_area.pos_x, unit_bytes) / unit_bytes;
    uint32_t out_img_width = XCAM_ALIGN_UP (out_desc.width, unit_bytes) / unit_bytes;
    uint32_t merge_width = XCAM_ALIGN_UP (merge_area.width, unit_bytes) / unit_bytes;
    cmds.push_back (new GLCmdUniformT<uint32_t> ("in_img_width", in_img_width));
    cmds.push_back (new GLCmdUniformT<uint32_t> ("in_img_height", in_desc.height));
    cmds.push_back (new GLCmdUniformT<uint32_t> ("in_offset_x", in_offset_x));
    cmds.push_back (new GLCmdUniformT<uint32_t> ("out_img_width", out_img_width));
    cmds.push_back (new GLCmdUniformT<uint32_t> ("merge_width", merge_width));

    GLGroupsSize groups_size;
    groups_size.x = XCAM_ALIGN_UP (out_img_width, 8) / 8;
    groups_size.y = XCAM_ALIGN_UP (out_desc.height, 16) / 16;
    groups_size.z = 1;

    SmartPtr<GLComputeProgram> prog;
    XCAM_FAIL_RETURN (
        ERROR, get_compute_program (prog), XCAM_RETURN_ERROR_PARAM,
        "GLGaussScalePyrShader(%s) get compute program failed", XCAM_STR (get_name ()));
    prog->set_groups_size (groups_size);

    return XCAM_RETURN_NO_ERROR;
}

bool
GLLapTransPyrShader::check_desc (
    const GLBufferDesc &in_desc, const GLBufferDesc &out_desc,
    const GLBufferDesc &gs_desc, const Rect &merge_area)
{
    XCAM_FAIL_RETURN (
        ERROR,
        merge_area.pos_y == 0 && merge_area.height == (int32_t)in_desc.height &&
        merge_area.pos_x + merge_area.width <= (int32_t)in_desc.width &&
        merge_area.width == (int32_t)out_desc.width && merge_area.height == (int32_t)out_desc.height &&
        merge_area.width == (int32_t)gs_desc.width * 2 && merge_area.height == (int32_t)gs_desc.height * 2,
        false,
        "invalid buffer size: intput:%dx%d, output:%dx%d, gaussscale:%dx%d, in_area:%dx%d",
        in_desc.width, in_desc.height, out_desc.width, out_desc.height,
        gs_desc.width, gs_desc.height, merge_area.width, merge_area.height);

    return true;
}

XCamReturn
GLLapTransPyrShader::prepare_arguments (const SmartPtr<Worker::Arguments> &base, GLCmdList &cmds)
{
    SmartPtr<GLLapTransPyrShader::Args> args = base.dynamic_cast_ptr<GLLapTransPyrShader::Args> ();
    XCAM_ASSERT (args.ptr () && args->in_glbuf.ptr () && args->gaussscale_glbuf.ptr () && args->out_glbuf.ptr ());

    const GLBufferDesc &in_desc = args->in_glbuf->get_buffer_desc ();
    const GLBufferDesc &gs_desc = args->gaussscale_glbuf->get_buffer_desc ();
    const GLBufferDesc &out_desc = args->out_glbuf->get_buffer_desc ();
    const Rect &merge_area = args->merge_area;
    XCAM_FAIL_RETURN (
        ERROR, check_desc (in_desc, out_desc, gs_desc, merge_area), XCAM_RETURN_ERROR_PARAM,
        "GLLapTransPyrShader(%s) check buffer description failed, level:%d idx:%d",
        XCAM_STR (get_name ()), args->level, (int)args->idx);

    cmds.push_back (new GLCmdBindBufRange (args->in_glbuf, 0, NV12PlaneYIdx));
    cmds.push_back (new GLCmdBindBufRange (args->in_glbuf, 1, NV12PlaneUVIdx));
    cmds.push_back (new GLCmdBindBufRange (args->gaussscale_glbuf, 2, NV12PlaneYIdx));
    cmds.push_back (new GLCmdBindBufRange (args->gaussscale_glbuf, 3, NV12PlaneUVIdx));
    cmds.push_back (new GLCmdBindBufRange (args->out_glbuf, 4, NV12PlaneYIdx));
    cmds.push_back (new GLCmdBindBufRange (args->out_glbuf, 5, NV12PlaneUVIdx));

    size_t unit_bytes = sizeof (uint32_t) * 2;
    uint32_t in_img_width = XCAM_ALIGN_UP (in_desc.width, unit_bytes) / unit_bytes;
    uint32_t in_offset_x = XCAM_ALIGN_UP (merge_area.pos_x, unit_bytes) / unit_bytes;
    uint32_t gaussscale_img_width = XCAM_ALIGN_UP (gs_desc.width, sizeof (uint32_t)) / sizeof (uint32_t);
    uint32_t merge_width = XCAM_ALIGN_UP (merge_area.width, unit_bytes) / unit_bytes;
    cmds.push_back (new GLCmdUniformT<uint32_t> ("in_img_width", in_img_width));
    cmds.push_back (new GLCmdUniformT<uint32_t> ("in_img_height", in_desc.height));
    cmds.push_back (new GLCmdUniformT<uint32_t> ("in_offset_x", in_offset_x));
    cmds.push_back (new GLCmdUniformT<uint32_t> ("gaussscale_img_width", gaussscale_img_width));
    cmds.push_back (new GLCmdUniformT<uint32_t> ("gaussscale_img_height", gs_desc.height));
    cmds.push_back (new GLCmdUniformT<uint32_t> ("merge_width", merge_width));

    GLGroupsSize groups_size;
    groups_size.x = XCAM_ALIGN_UP (merge_width, 8) / 8;
    groups_size.y = XCAM_ALIGN_UP (merge_area.height, 32) / 32;
    groups_size.z = 1;

    SmartPtr<GLComputeProgram> prog;
    XCAM_FAIL_RETURN (
        ERROR, get_compute_program (prog), XCAM_RETURN_ERROR_PARAM,
        "GLLapTransPyrShader(%s) get compute program failed", XCAM_STR (get_name ()));
    prog->set_groups_size (groups_size);

    return XCAM_RETURN_NO_ERROR;
}

bool
GLBlendPyrShader::check_desc (
    const GLBufferDesc &in0_desc, const GLBufferDesc &in1_desc,
    const GLBufferDesc &out_desc, const GLBufferDesc &mask_desc,
    const Rect &in0_area, const Rect &in1_area, const Rect &out_area)
{
    XCAM_FAIL_RETURN (
        ERROR,
        (int32_t)in0_desc.width >= in0_area.width && (int32_t)in0_desc.height == in0_area.height &&
        (int32_t)in1_desc.width >= in1_area.width && (int32_t)in1_desc.height == in1_area.height &&
        (int32_t)out_desc.width >= out_area.width && (int32_t)out_desc.height == out_area.height &&
        (int32_t)mask_desc.width == in0_area.width && (int32_t)mask_desc.width == in1_area.width &&
        (int32_t)mask_desc.width == out_area.width && in0_area.height == in1_area.height &&
        in1_area.height == out_area.height,
        false,
        "invalid buffer size: intput0:%dx%d, intput1:%dx%d, output:%dx%d, mask:%dx%d, "
        "in0_area:%dx%d, in1_area:%dx%d, out_area:%dx%d",
        in0_desc.width, in0_desc.height, in1_desc.width, in1_desc.height,
        out_desc.width, out_desc.height, mask_desc.width, mask_desc.height,
        in0_area.width, in0_area.height, in1_area.width, in1_area.height, out_area.width, out_area.height);

    XCAM_FAIL_RETURN (
        ERROR, mask_desc.height == 1, false,
        "mask buffer only supports one-dimensional array");

    return true;
}

XCamReturn
GLBlendPyrShader::prepare_arguments (const SmartPtr<Worker::Arguments> &base, GLCmdList &cmds)
{
    SmartPtr<GLBlendPyrShader::Args> args = base.dynamic_cast_ptr<GLBlendPyrShader::Args> ();
    XCAM_ASSERT (args.ptr () && args->in0_glbuf.ptr () && args->in1_glbuf.ptr () && args->out_glbuf.ptr ());
    XCAM_ASSERT (args->mask_glbuf.ptr ());

    const GLBufferDesc &in0_desc = args->in0_glbuf->get_buffer_desc ();
    const GLBufferDesc &in1_desc = args->in1_glbuf->get_buffer_desc ();
    const GLBufferDesc &out_desc = args->out_glbuf->get_buffer_desc ();
    const GLBufferDesc &mask_desc = args->mask_glbuf->get_buffer_desc ();
    XCAM_FAIL_RETURN (
        ERROR, check_desc (in0_desc, in1_desc, out_desc, mask_desc, args->in0_area, args->in1_area, args->out_area),
        XCAM_RETURN_ERROR_PARAM,
        "GLBlendPyrShader(%s) check buffer description failed", XCAM_STR (get_name ()));

    cmds.push_back (new GLCmdBindBufRange (args->in0_glbuf, 0, NV12PlaneYIdx));
    cmds.push_back (new GLCmdBindBufRange (args->in0_glbuf, 1, NV12PlaneUVIdx));
    cmds.push_back (new GLCmdBindBufRange (args->in1_glbuf, 2, NV12PlaneYIdx));
    cmds.push_back (new GLCmdBindBufRange (args->in1_glbuf, 3, NV12PlaneUVIdx));
    cmds.push_back (new GLCmdBindBufRange (args->out_glbuf, 4, NV12PlaneYIdx));
    cmds.push_back (new GLCmdBindBufRange (args->out_glbuf, 5, NV12PlaneUVIdx));
    cmds.push_back (new GLCmdBindBufBase (args->mask_glbuf, 6));

    size_t unit_bytes = sizeof (uint32_t) * 2;
    uint32_t in0_img_width = XCAM_ALIGN_UP (in0_desc.width, unit_bytes) / unit_bytes;
    uint32_t in1_img_width = XCAM_ALIGN_UP (in1_desc.width, unit_bytes) / unit_bytes;
    uint32_t out_img_width = XCAM_ALIGN_UP (out_desc.width, unit_bytes) / unit_bytes;

    uint32_t blend_width = XCAM_ALIGN_UP (mask_desc.width, unit_bytes) / unit_bytes;
    uint32_t in0_offset_x = XCAM_ALIGN_UP (args->in0_area.pos_x, unit_bytes) / unit_bytes;
    uint32_t in1_offset_x = XCAM_ALIGN_UP (args->in1_area.pos_x, unit_bytes) / unit_bytes;
    uint32_t out_offset_x = XCAM_ALIGN_UP (args->out_area.pos_x, unit_bytes) / unit_bytes;

    cmds.push_back (new GLCmdUniformT<uint32_t> ("in0_img_width", in0_img_width));
    cmds.push_back (new GLCmdUniformT<uint32_t> ("in1_img_width", in1_img_width));
    cmds.push_back (new GLCmdUniformT<uint32_t> ("out_img_width", out_img_width));

    cmds.push_back (new GLCmdUniformT<uint32_t> ("blend_width", blend_width));
    cmds.push_back (new GLCmdUniformT<uint32_t> ("in0_offset_x", in0_offset_x));
    cmds.push_back (new GLCmdUniformT<uint32_t> ("in1_offset_x", in1_offset_x));
    cmds.push_back (new GLCmdUniformT<uint32_t> ("out_offset_x", out_offset_x));

    GLGroupsSize groups_size;
    groups_size.x = XCAM_ALIGN_UP (args->in0_area.width, 8) / 8;
    groups_size.y = XCAM_ALIGN_UP (args->in0_area.height, 16) / 16;
    groups_size.z = 1;

    SmartPtr<GLComputeProgram> prog;
    XCAM_FAIL_RETURN (
        ERROR, get_compute_program (prog), XCAM_RETURN_ERROR_PARAM,
        "GLBlendPyrShader(%s) get compute program failed", XCAM_STR (get_name ()));
    prog->set_groups_size (groups_size);

    return XCAM_RETURN_NO_ERROR;
}

bool
GLReconstructPyrShader::check_desc (
    const GLBufferDesc &lap0_desc, const GLBufferDesc &lap1_desc, const GLBufferDesc &out_desc,
    const GLBufferDesc &prev_blend_desc, const GLBufferDesc &mask_desc, const Rect &merge_area)
{
    XCAM_FAIL_RETURN (
        ERROR,
        merge_area.pos_y == 0 && merge_area.height == (int32_t)out_desc.height &&
        merge_area.pos_x + merge_area.width <= (int32_t)out_desc.width &&
        merge_area.width == (int32_t)lap0_desc.width && merge_area.height == (int32_t)lap0_desc.height &&
        lap0_desc.width == lap1_desc.width && lap0_desc.height == lap1_desc.height &&
        lap0_desc.width == prev_blend_desc.width * 2 && lap0_desc.height == prev_blend_desc.height * 2 &&
        lap0_desc.width == mask_desc.width,
        false,
        "invalid buffer size: lap0:%dx%d, lap1:%dx%d, output:%dx%d, prev_blend:%dx%d, mask:%dx%d, merge_area:%dx%d",
        lap0_desc.width, lap0_desc.height, lap1_desc.width, lap1_desc.height,
        out_desc.width, out_desc.height, prev_blend_desc.width, prev_blend_desc.height,
        mask_desc.width, mask_desc.height, merge_area.width, merge_area.height);

    XCAM_FAIL_RETURN (
        ERROR, mask_desc.height == 1, false,
        "mask buffer only supports one-dimensional array");

    return true;
}

XCamReturn
GLReconstructPyrShader::prepare_arguments (const SmartPtr<Worker::Arguments> &base, GLCmdList &cmds)
{
    SmartPtr<GLReconstructPyrShader::Args> args = base.dynamic_cast_ptr<GLReconstructPyrShader::Args> ();
    XCAM_ASSERT (args.ptr () && args->lap0_glbuf.ptr () && args->lap1_glbuf.ptr () && args->out_glbuf.ptr ());
    XCAM_ASSERT (args->mask_glbuf.ptr ());

    const GLBufferDesc &lap0_desc = args->lap0_glbuf->get_buffer_desc ();
    const GLBufferDesc &lap1_desc = args->lap1_glbuf->get_buffer_desc ();
    const GLBufferDesc &out_desc = args->out_glbuf->get_buffer_desc ();
    const GLBufferDesc &prev_blend_desc = args->prev_blend_glbuf->get_buffer_desc ();
    const GLBufferDesc &mask_desc = args->mask_glbuf->get_buffer_desc ();
    const Rect &merge_area = args->merge_area;
    XCAM_FAIL_RETURN (
        ERROR, check_desc (lap0_desc, lap1_desc, out_desc, prev_blend_desc, mask_desc, merge_area),
        XCAM_RETURN_ERROR_PARAM,
        "GLReconstructPyrShader(%s) check buffer description failed, level:%d",
        XCAM_STR (get_name ()), args->level);

    cmds.push_back (new GLCmdBindBufRange (args->lap0_glbuf, 0, NV12PlaneYIdx));
    cmds.push_back (new GLCmdBindBufRange (args->lap0_glbuf, 1, NV12PlaneUVIdx));
    cmds.push_back (new GLCmdBindBufRange (args->lap1_glbuf, 2, NV12PlaneYIdx));
    cmds.push_back (new GLCmdBindBufRange (args->lap1_glbuf, 3, NV12PlaneUVIdx));
    cmds.push_back (new GLCmdBindBufRange (args->out_glbuf, 4, NV12PlaneYIdx));
    cmds.push_back (new GLCmdBindBufRange (args->out_glbuf, 5, NV12PlaneUVIdx));
    cmds.push_back (new GLCmdBindBufRange (args->prev_blend_glbuf, 6, NV12PlaneYIdx));
    cmds.push_back (new GLCmdBindBufRange (args->prev_blend_glbuf, 7, NV12PlaneUVIdx));
    cmds.push_back (new GLCmdBindBufBase (args->mask_glbuf, 8));

    size_t unit_bytes = sizeof (uint32_t) * 2;
    uint32_t lap_img_width = XCAM_ALIGN_UP (lap0_desc.width, unit_bytes) / unit_bytes;
    uint32_t out_img_width = XCAM_ALIGN_UP (out_desc.width, unit_bytes) / unit_bytes;
    uint32_t out_offset_x = XCAM_ALIGN_UP (merge_area.pos_x, unit_bytes) / unit_bytes;
    uint32_t prev_blend_img_width = XCAM_ALIGN_UP (prev_blend_desc.width, sizeof (uint32_t)) / sizeof (uint32_t);
    cmds.push_back (new GLCmdUniformT<uint32_t> ("lap_img_width", lap_img_width));
    cmds.push_back (new GLCmdUniformT<uint32_t> ("lap_img_height", lap0_desc.height));
    cmds.push_back (new GLCmdUniformT<uint32_t> ("out_img_width", out_img_width));
    cmds.push_back (new GLCmdUniformT<uint32_t> ("out_offset_x", out_offset_x));
    cmds.push_back (new GLCmdUniformT<uint32_t> ("prev_blend_img_width", prev_blend_img_width));
    cmds.push_back (new GLCmdUniformT<uint32_t> ("prev_blend_img_height", prev_blend_desc.height));

    GLGroupsSize groups_size;
    groups_size.x = XCAM_ALIGN_UP (lap_img_width, 8) / 8;
    groups_size.y = XCAM_ALIGN_UP (lap0_desc.height, 32) / 32;
    groups_size.z = 1;

    SmartPtr<GLComputeProgram> prog;
    XCAM_FAIL_RETURN (
        ERROR, get_compute_program (prog), XCAM_RETURN_ERROR_PARAM,
        "GLReconstructPyrShader(%s) get compute program failed", XCAM_STR (get_name ()));
    prog->set_groups_size (groups_size);

    return XCAM_RETURN_NO_ERROR;
}

SmartPtr<GLGaussScalePyrShader>
create_gauss_scale_pyr_shader (SmartPtr<Worker::Callback> &cb)
{
    XCAM_ASSERT (cb.ptr ());

    SmartPtr<GLGaussScalePyrShader> shader = new GLGaussScalePyrShader (cb);
    XCAM_ASSERT (shader.ptr ());

    XCamReturn ret = shader->create_compute_program (shaders_info[ShaderGaussScalePyr], "gauss_scale_pyr_program");
    XCAM_FAIL_RETURN (
        ERROR, ret == XCAM_RETURN_NO_ERROR, NULL,
        "create gauss scale pyramid program failed");

    return shader;
}

SmartPtr<GLLapTransPyrShader>
create_lap_trans_pyr_shader (SmartPtr<Worker::Callback> &cb)
{
    XCAM_ASSERT (cb.ptr ());

    SmartPtr<GLLapTransPyrShader> shader = new GLLapTransPyrShader (cb);
    XCAM_ASSERT (shader.ptr ());

    XCamReturn ret = shader->create_compute_program (shaders_info[ShaderLapTransPyr], "lap_trans_pyr_program");
    XCAM_FAIL_RETURN (
        ERROR, ret == XCAM_RETURN_NO_ERROR, NULL,
        "create laplace transformation pyramid program failed");

    return shader;
}

SmartPtr<GLBlendPyrShader>
create_blend_pyr_shader (SmartPtr<Worker::Callback> &cb)
{
    XCAM_ASSERT (cb.ptr ());

    SmartPtr<GLBlendPyrShader> shader = new GLBlendPyrShader (cb);
    XCAM_ASSERT (shader.ptr ());

    XCamReturn ret = shader->create_compute_program (shaders_info[ShaderBlendPyr], "blend_pyr_program");
    XCAM_FAIL_RETURN (
        ERROR, ret == XCAM_RETURN_NO_ERROR, NULL,
        "create blend pyramid program failed");

    return shader;
}

SmartPtr<GLReconstructPyrShader>
create_reconstruct_pyr_shader (SmartPtr<Worker::Callback> &cb)
{
    XCAM_ASSERT (cb.ptr ());

    SmartPtr<GLReconstructPyrShader> shader = new GLReconstructPyrShader (cb);
    XCAM_ASSERT (shader.ptr ());

    XCamReturn ret = shader->create_compute_program (shaders_info[ShaderReconstructPyr], "reconstruct_pyr_program");
    XCAM_FAIL_RETURN (
        ERROR, ret == XCAM_RETURN_NO_ERROR, NULL,
        "create reconstruct pyramid program failed");

    return shader;
}

}

}

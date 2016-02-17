#include "graph.h"



//
//typedef bool (*flow_nodedef_fn_stringify)(Context *c, struct flow_graph *g, int32_t node_id, char * buffer, size_t buffer_size);
//
//
//
//typedef bool (*flow_nodedef_fn_infobyte_count)(Context *c, struct flow_graph *g, int32_t node_id, int32_t * infobytes_count_out);
//
//typedef bool (*flow_nodedef_fn_populate_dimensions)(Context *c, struct flow_graph *g, int32_t node_id, int32_t outbound_edge_id);
//
//
//typedef bool (*flow_nodedef_fn_flatten)(Context *c, struct flow_graph **graph_ref, int32_t node_id);
//
//typedef bool (*flow_nodedef_fn_execute)(Context *c, struct flow_graph *g, int32_t node_id);
//
//
//
//
//struct flow_node_definition{
//    flow_ntype type;
//    int32_t input_count;
//    int32_t canvas_count;
//    const char * type_name;
//
//    flow_nodedef_fn_stringify stringify;
//    flow_nodedef_fn_infobyte_count count_infobytes;
//    int32_t nodeinfo_bytes_fixed;
//    flow_nodedef_fn_populate_dimensions populate_dimensions;
//    flow_nodedef_fn_flatten flatten;
//    flow_nodedef_fn_execute execute;
//
//};
#define FLOW_GET_INFOBYTES(g, node_id, type, varname) struct type * varname = (struct type *) &g->info_bytes[g->nodes[node_id].info_byte_index];

#define FLOW_GET_INPUT_EDGE(g, node_id) int32_t input_edge_id = flow_graph_get_first_inbound_edge_of_type(c,g,node_id, flow_edgetype_input); \
    if (input_edge_id < 0) { \
        CONTEXT_error(c, Invalid_inputs_to_node); \
        return false; \
    } \
    struct flow_edge * input_edge = &g->edges[input_edge_id];


#define FLOW_GET_CANVAS_EDGE(g, node_id) int32_t canvas_edge_id = flow_graph_get_first_inbound_edge_of_type(c,g,node_id, flow_edgetype_canvas); \
    if (canvas_edge_id < 0) { \
        CONTEXT_error(c, Invalid_inputs_to_node); \
        return false; \
    } \
    struct flow_edge * canvas_edge = &g->edges[canvas_edge_id];



static const char * get_format_name(BitmapPixelFormat f, bool alpha_meaningful){
    switch(f){
        case Bgr24: return "Bgr24";
        case Bgra32: return alpha_meaningful ? "Bgra32" : "Bgr32";
        default: return "?";
    }
}


static bool stringify_scale(Context *c, struct flow_graph *g, int32_t node_id, char * buffer, size_t buffer_size){
    FLOW_GET_INFOBYTES(g,node_id, flow_nodeinfo_size, info);

    snprintf(buffer, buffer_size, "scale %lux%lu%s", info->width, info->height, g->nodes[node_id].executed ? " [done]" : "");
    return true;
}

static bool stringify_canvas(Context *c, struct flow_graph *g, int32_t node_id, char * buffer, size_t buffer_size){
    FLOW_GET_INFOBYTES(g,node_id, flow_nodeinfo_createcanvas, info);

    snprintf(buffer, buffer_size, "create canvas %lux%lu %s%s", info->width, info->height, get_format_name(info->format, false), g->nodes[node_id].executed ? " [done]" : "");
    return true;
}

static bool stringify_render1d(Context *c, struct flow_graph *g, int32_t node_id, char * buffer, size_t buffer_size){
    FLOW_GET_INFOBYTES(g,node_id, flow_nodeinfo_render_to_canvas_1d, info);

    //TODO
    snprintf(buffer, buffer_size, "render1d %d%s", info->scale_to_width, g->nodes[node_id].executed ? " [done]" : "");
    return true;
}

static bool stringify_placeholder(Context *c, struct flow_graph *g, int32_t node_id, char * buffer, size_t buffer_size){
    FLOW_GET_INFOBYTES(g,node_id, flow_nodeinfo_index, info);

    snprintf(buffer, buffer_size, "resource placeholder #%d", info->index);
    return true;
}


static bool stringify_bitmap_bgra_pointer(Context *c, struct flow_graph *g, int32_t node_id, char * buffer, size_t buffer_size){
    snprintf(buffer, buffer_size, "BitmapBgra container");
    return true;
}

static bool stringify_decode_png(Context *c, struct flow_graph *g, int32_t node_id, char * buffer, size_t buffer_size){
    FLOW_GET_INFOBYTES(g,node_id, flow_nodeinfo_decode_png, info);

    //TODO
    snprintf(buffer, buffer_size, "decode png %lux%lu%s", info->width, info->height, g->nodes[node_id].executed ? " [done]" : "");
    return true;
}




static bool dimensions_scale(Context *c, struct flow_graph *g, int32_t node_id, int32_t outbound_edge_id, bool force_estimate){
    FLOW_GET_INFOBYTES(g,node_id, flow_nodeinfo_size, info)
    FLOW_GET_INPUT_EDGE(g,node_id)

    struct flow_edge * output = &g->edges[outbound_edge_id];

    output->from_width = info->width;
    output->from_height = info->height;
    output->from_alpha_meaningful = input_edge->from_alpha_meaningful;
    output->from_format = input_edge->from_format;
    return true;
}

static bool dimensions_canvas(Context *c, struct flow_graph *g, int32_t node_id, int32_t outbound_edge_id, bool force_estimate){
    FLOW_GET_INFOBYTES(g,node_id, flow_nodeinfo_createcanvas, info)

    struct flow_edge * output = &g->edges[outbound_edge_id];

    output->from_width = info->width;
    output->from_height = info->height;
    output->from_alpha_meaningful = false;
    output->from_format = info->format;
    return true;
}
static bool dimensions_render1d(Context *c, struct flow_graph *g, int32_t node_id, int32_t outbound_edge_id, bool force_estimate){
    //FLOW_GET_INFOBYTES(g,node_id, flow_nodeinfo_size, info)
    FLOW_GET_CANVAS_EDGE(g,node_id)

    struct flow_edge * output = &g->edges[outbound_edge_id];

    output->from_format = Bgra32; //TODO: maybe wrong
    output->from_alpha_meaningful = true; //TODO: WRONG! Involve "input" in decision
    output->from_width = canvas_edge->from_width;
    output->from_height = canvas_edge->from_height;
    return true;
}

static bool dimensions_decode_png(Context *c, struct flow_graph *g, int32_t node_id, int32_t outbound_edge_id, bool force_estimate){
    FLOW_GET_INFOBYTES(g,node_id, flow_nodeinfo_decode_png, info)

    struct flow_edge * output = &g->edges[outbound_edge_id];

    output->from_width = info->width;
    output->from_height = info->height;
    output->from_alpha_meaningful = true;
    output->from_format = Bgra32;
    return true;
}

static bool flattenshort_scale(Context *c, struct flow_graph **g, int32_t node_id, struct flow_node * node, struct flow_edge * input_edge, int32_t * first_replacement_node, int32_t * last_replacement_node){
    FLOW_GET_INFOBYTES((*g),node_id, flow_nodeinfo_size, size)

    //create canvas for render1d
    int32_t canvas_a = flow_node_create_canvas(c,g,-1,input_edge->from_format,input_edge->from_height, size->width,0);
    if (canvas_a < 0){
        CONTEXT_error_return(c);
    }
    int32_t canvas_b = flow_node_create_canvas(c,g,-1,input_edge->from_format,size->width,size->height,0);
    if (canvas_b < 0){
        CONTEXT_error_return(c);
    }

    WorkingFloatspace floatspace = Floatspace_linear;
    flow_compositing_mode mode = flow_compositing_mode_overwrite;
    InterpolationFilter filter = Filter_Robidoux;
    uint8_t *matte_color[4];
    float sharpen_percent =0;

    *first_replacement_node = flow_node_create_render_to_canvas_1d(c,g,-1,true,0,0,size->width, floatspace, sharpen_percent, mode, matte_color, NULL, filter);
    if (*first_replacement_node < 0){
        CONTEXT_error_return(c);
    }
    if (flow_edge_create(c,g, canvas_a, *first_replacement_node, flow_edgetype_canvas) < 0){
        CONTEXT_error_return(c);
    }

    *last_replacement_node = flow_node_create_render_to_canvas_1d(c,g, *first_replacement_node,true, 0,0, size->height, floatspace, sharpen_percent, mode, matte_color, NULL, filter);
    if (*last_replacement_node < 0){
        CONTEXT_error_return(c);
    }
    if (flow_edge_create(c,g, canvas_b, *last_replacement_node, flow_edgetype_canvas) < 0){
        CONTEXT_error_return(c);
    }
    return true;
}


static bool execute_canvas(Context *c, struct flow_job * job, struct flow_graph * g, int32_t node_id){
    FLOW_GET_INFOBYTES(g,node_id, flow_nodeinfo_createcanvas, info)

    struct flow_node * n = &g->nodes[node_id];
    //TODO: bgcolor
    n->result_bitmap = BitmapBgra_create(c, info->width, info->height, true, info->format);
    if ( n->result_bitmap == NULL){
        CONTEXT_error_return(c);
    }
    n->executed = true;
    return true;
}

static bool execute_bitmap_bgra_pointer(Context *c, struct flow_job * job,struct flow_graph * g, int32_t node_id){
    FLOW_GET_INFOBYTES(g,node_id, flow_nodeinfo_resource_bitmap_bgra, info)
    FLOW_GET_INPUT_EDGE(g,node_id)
    struct flow_node * n = &g->nodes[node_id];
    *info->ref = n->result_bitmap = g->nodes[input_edge->from].result_bitmap;
    n->executed = true;
    return true;
}

static bool execute_render1d(Context *c, struct flow_job * job, struct flow_graph * g, int32_t node_id){
    FLOW_GET_INFOBYTES(g,node_id, flow_nodeinfo_render_to_canvas_1d, info)
    FLOW_GET_INPUT_EDGE(g,node_id)
    FLOW_GET_CANVAS_EDGE(g,node_id)
    struct flow_node * n = &g->nodes[node_id];

    BitmapBgra * input = g->nodes[input_edge->from].result_bitmap;
    BitmapBgra * canvas = g->nodes[canvas_edge->from].result_bitmap;

    if (!flow_node_execute_render_to_canvas_1d(c, job, input, canvas, info)){
        CONTEXT_error_return(c);
    }
    n->result_bitmap = canvas;
    n->executed = true;
    return true;
}

static bool execute_decode_png(Context *c, struct flow_job * job, struct flow_graph * g, int32_t node_id){
    FLOW_GET_INFOBYTES(g,node_id, flow_nodeinfo_decode_png, info)

    struct flow_node * n = &g->nodes[node_id];
    //TODO: bgcolor
    n->result_bitmap = BitmapBgra_create(c, info->width, info->height, true, Bgra32);
    if ( n->result_bitmap == NULL){
        CONTEXT_error_return(c);
    }
    n->executed = true;
    return true;
}



struct flow_node_definition flow_node_defs[] = {
        {
                .type = flow_ntype_Scale,
                .input_count = 1,
                .canvas_count = 0,
                .type_name = "scale",
                .nodeinfo_bytes_fixed = sizeof(struct flow_nodeinfo_size),
                .count_infobytes = NULL,
                .stringify = stringify_scale,
                .populate_dimensions = dimensions_scale,
                .flatten_shorthand = flattenshort_scale,
                .execute = NULL

        },
        {
                .type = flow_ntype_Create_Canvas,
                .nodeinfo_bytes_fixed = sizeof(struct flow_nodeinfo_createcanvas),
                .input_count = 0,
                .canvas_count = 0,
                .populate_dimensions = dimensions_canvas,
                .type_name = "create canvas",
                .stringify = stringify_canvas,
                .execute = execute_canvas


        },
        {
                .type = flow_ntype_Resource_Placeholder,
                .nodeinfo_bytes_fixed = sizeof(struct flow_nodeinfo_index),
                .type_name = "resource placeholder",
                .input_count = -1,
                .canvas_count = 0,
                .stringify = stringify_placeholder


        },
        {
                .type = flow_ntype_primitive_bitmap_bgra_pointer,
                .nodeinfo_bytes_fixed = sizeof(struct flow_nodeinfo_resource_bitmap_bgra),
                .type_name = "bitmap bgra pointer",
                .input_count = 1,
                .canvas_count = 0,
                .stringify = stringify_bitmap_bgra_pointer,
                .execute = execute_bitmap_bgra_pointer,

        },
        {
                .type = flow_ntype_primitive_RenderToCanvas1D,
                .nodeinfo_bytes_fixed = sizeof(struct flow_nodeinfo_render_to_canvas_1d),
                .type_name = "render to canvas 1d",
                .input_count = 1,
                .canvas_count = 1,
                .stringify = stringify_render1d,
                .populate_dimensions = dimensions_render1d,
                .execute = execute_render1d


        },
        {
                .type = flow_ntype_primitive_Decode_Png,
                .nodeinfo_bytes_fixed = sizeof(struct flow_nodeinfo_decode_png),
                .type_name = "decode png",
                .input_count = 0,
                .canvas_count = 0, //?
                .stringify = stringify_decode_png,
                .populate_dimensions = dimensions_decode_png,
                .execute = execute_decode_png,

        },
        {
                .type= flow_ntype_Null,
                .type_name = "(null)",
                .input_count = 0,
                .canvas_count = 0,

        }
};
int32_t flow_node_defs_count = sizeof(flow_node_defs) / sizeof(struct flow_node_definition);

struct flow_node_definition * flow_nodedef_get(Context *c, flow_ntype type){
    int i = 0;
    for (i = 0; i < flow_node_defs_count; i++){
        if (flow_node_defs[i].type == type) return &flow_node_defs[i];
    }
    CONTEXT_error(c, Not_implemented);
    return NULL;
}


bool flow_node_stringify(Context *c, struct flow_graph *g, int32_t node_id, char * buffer, size_t buffer_size){
    struct flow_node * node = &g->nodes[node_id];
    struct flow_node_definition * def = flow_nodedef_get(c,node->type);
    if (def == NULL){
        CONTEXT_error_return(c);
    }
    if (def->stringify == NULL){
        if (def->type_name == NULL){
            CONTEXT_error(c,Not_implemented);
            return false;
        }
        snprintf(buffer,buffer_size,"%s%s",def->type_name, node->executed ? " [done]" : "");
    }else{
        def->stringify(c,g,node_id, buffer,buffer_size);
    }
    return true;
}
int32_t flow_node_fixed_infobyte_count(Context *c, flow_ntype type){
    struct flow_node_definition * def = flow_nodedef_get(c,type);
    if (def == NULL){
        CONTEXT_add_to_callstack(c);
        return -1;
    }
    if (def->nodeinfo_bytes_fixed < 0){
        CONTEXT_error(c,Not_implemented);
    }
    return def->nodeinfo_bytes_fixed;
}
bool flow_node_infobyte_count(Context *c, struct flow_graph *g, int32_t node_id, int32_t * infobytes_count_out){
    struct flow_node * node = &g->nodes[node_id];
    struct flow_node_definition * def = flow_nodedef_get(c,node->type);
    if (def == NULL){
        CONTEXT_error_return(c);
    }
    if (def->count_infobytes == NULL){
        *infobytes_count_out = flow_node_fixed_infobyte_count(c,node->type);
        if (*infobytes_count_out < 0){
            CONTEXT_error_return(c);
        }
    }else{
        def->count_infobytes(c,g,node_id, infobytes_count_out);
    }
    return true;
}

bool flow_node_validate_inputs(Context *c, struct flow_graph *g, int32_t node_id){
    struct flow_node * node = &g->nodes[node_id];
    struct flow_node_definition * def = flow_nodedef_get(c,node->type);
    if (def == NULL){
        CONTEXT_error_return(c);
    }

    int32_t input_edge_count = flow_graph_get_inbound_edge_count_of_type(c,g,node_id, flow_edgetype_input);
    int32_t canvas_edge_count = flow_graph_get_inbound_edge_count_of_type(c,g,node_id, flow_edgetype_canvas);

    if (def->input_count > -1 && def->input_count != input_edge_count){
        CONTEXT_error(c, Invalid_inputs_to_node);
        return false;
    }
    if (def->canvas_count > -1 && def->canvas_count != canvas_edge_count){
        CONTEXT_error(c, Invalid_inputs_to_node);
        return false;
    }
    return true;
}
bool flow_node_populate_dimensions_to_edge(Context *c, struct flow_graph *g, int32_t node_id, int32_t outbound_edge_id, bool force_estimate){
    if (!flow_node_validate_inputs(c,g,node_id)){
        CONTEXT_error_return(c);
    }
    struct flow_node * node = &g->nodes[node_id];
    struct flow_node_definition * def = flow_nodedef_get(c,node->type);
    if (def == NULL){
        CONTEXT_error_return(c);
    }
    if (def->populate_dimensions == NULL){
        CONTEXT_error(c,Not_implemented);
        return false;
    }else{
        def->populate_dimensions(c,g,node_id,outbound_edge_id, force_estimate);
    }
    return true;
}
bool flow_node_flatten(Context *c, struct flow_graph **graph_ref, int32_t node_id){
    if (!flow_node_validate_inputs(c,*graph_ref,node_id)){
        CONTEXT_error_return(c);
    }
    struct flow_node * node = &(*graph_ref)->nodes[node_id];
    struct flow_node_definition * def = flow_nodedef_get(c,node->type);
    if (def == NULL){
        CONTEXT_error_return(c);
    }
    if (def->flatten == NULL){
        if (def->flatten_shorthand == NULL) {
            CONTEXT_error(c, Not_implemented);
            return false;
        }else{
            int32_t first_replacement_node = -1;
            int32_t last_replacement_node = -1;

            int32_t input_edge_id = flow_graph_get_first_inbound_edge_of_type(c,*graph_ref,node_id, flow_edgetype_input);
            struct flow_edge * input_edge = input_edge_id < 0 ? NULL : &(*graph_ref)->edges[input_edge_id];


            def->flatten_shorthand(c,graph_ref,node_id, node, input_edge, &first_replacement_node,&last_replacement_node);

            //Clone edges
            if (!flow_graph_duplicate_edges_to_another_node(c,graph_ref,node_id, first_replacement_node, true, false)){
                CONTEXT_error_return(c);
            }
            if (!flow_graph_duplicate_edges_to_another_node(c,graph_ref,node_id, last_replacement_node, false, true)){
                CONTEXT_error_return(c);
            }

            //Delete the original
            if (!flow_node_delete(c,*graph_ref, node_id)){
                CONTEXT_error_return(c);
            }
        }
    }else{
        def->flatten(c,graph_ref,node_id);
    }
    return true;
}
bool flow_node_execute(Context *c, struct flow_job * job, struct flow_graph *g, int32_t node_id){
    if (!flow_node_validate_inputs(c,g,node_id)){
        CONTEXT_error_return(c);
    }
    struct flow_node * node = &g->nodes[node_id];
    struct flow_node_definition * def = flow_nodedef_get(c,node->type);
    if (def == NULL){
        CONTEXT_error_return(c);
    }
    if (def->execute == NULL){
        CONTEXT_error(c,Not_implemented);
        return false;
    }else{
        def->execute(c,job, g,node_id);
    }
    return true;
}
bool flow_node_estimate_execution_cost(Context *c, struct flow_graph *g, int32_t node_id, size_t * bytes_required, size_t * cpu_cost){
    CONTEXT_error(c, Not_implemented);
    return false;
}
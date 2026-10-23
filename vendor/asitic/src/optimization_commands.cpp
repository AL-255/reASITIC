#include "asitic/optimization_commands.hpp"
namespace asitic {
bool execute_optimization_command(int id,CommandArguments& arguments,OptimizationCommandContext& context,bool* suppress_recording) {
    auto& backends=context.backends;
    switch(id) {
    case 700:
        if(!backends.square || !context.square || !context.square_options)return false;
        backends.square(*context.square,*context.square_options,0);break;
    case 706:if(!backends.area)return false;backends.area(backends.user);break;
    case 707:if(!backends.batch)return false;backends.batch(backends.user);break;
    case 708:if(!backends.polygon)return false;backends.polygon(0,backends.user);break;
    case 713:if(!backends.symmetric_square)return false;backends.symmetric_square(0,backends.user);break;
    case 714:if(!backends.symmetric_polygon)return false;backends.symmetric_polygon(0,backends.user);break;
    case 711:
    case 715: {
        if((id==711 && !backends.grid) || (id==715 && !backends.multimetal_grid) || !context.grid || !context.grid_options)return false;
        const bool batch=arguments.command.value==1u;
        if(batch) {
            if(!backends.arguments || !context.arguments_history)return false;
            backends.arguments(arguments.arguments[0].text,context.grid->settings,context.grid_options->prompts,*context.arguments_history);
        }
        if(id==711)backends.grid(*context.grid,*context.grid_options,batch?1:0);
        else backends.multimetal_grid(batch?1:0,backends.user);
        break;
    }
    default:return false;
    }
    if(suppress_recording)*suppress_recording=false;
    return true;
}
}

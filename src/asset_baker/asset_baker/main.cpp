#include <cstdint>
#include <tclap/CmdLine.h>
#include <spdlog/spdlog.h>
#include <filesystem>
#include "asset_baker/gltf_loader.hpp"
#include <fstream>
#include <shared/serialized_asset_formats.hpp>
#include <TaskScheduler.h>

namespace asset_baker
{

struct Asset_Bake_Context
{
    std::filesystem::path input_directory;
    std::filesystem::path output_directory;
    // bool use_cache;
    enki::TaskScheduler task_scheduler;
};

void process_gltf(Asset_Bake_Context& context, const std::filesystem::path& input_file)
{
    spdlog::info("Processing GLTF file '{}'", input_file.string());
    // TODO: check cache
    auto gltf = process_gltf_from_file(input_file);
    if (gltf.has_value())
    {
        {
            const auto serialized_model = serialize_gltf_model(input_file.filename().string(), gltf.value());
            const auto outfile_path = (context.output_directory / input_file.stem()).string() + serialization::MODEL_FILE_EXTENSION;
            if (!std::filesystem::exists(outfile_path))
            {
                spdlog::info("Directory '{}' does not exist, creating it.", context.output_directory.string());
                std::filesystem::create_directory(context.output_directory);
            }
            std::ofstream outfile(outfile_path, std::ios::binary | std::ios::out);
            outfile.write(serialized_model.data(), static_cast<std::streamsize>(serialized_model.size()));
            outfile.close();
            spdlog::info("Successfully processed GLTF file '{}' and written it to '{}'",
                input_file.string(),
                outfile_path);
        }

        spdlog::debug("Processing textures.");

        std::vector<std::unique_ptr<enki::TaskSet>> tasks;
        tasks.reserve(gltf.value().texture_load_requests.size());
        for (auto& request : gltf.value().texture_load_requests)
        {
            auto& task = tasks.emplace_back(std::make_unique<enki::TaskSet>(
                1,
                [&](enki::TaskSetPartition range, uint32_t thread_idx)
                {
                    spdlog::info("Processing texture '{}' with hash '{}'",
                        request.name,
                        std::string(request.hash_identifier, serialization::HASH_IDENTIFIER_FIELD_SIZE));

                    auto texture_data = process_and_serialize_gltf_texture(request);

                    if (texture_data.empty())
                    {
                        spdlog::debug("Skipping texture write");
                        return;
                    }

                    const auto outfile_path = (context.output_directory
                        / std::string(request.hash_identifier, serialization::HASH_IDENTIFIER_FIELD_SIZE)).string()
                        + serialization::TEXTURE_FILE_EXTENSION;

                    std::ofstream outfile(outfile_path, std::ios::binary | std::ios::out);
                    outfile.write(texture_data.data(), static_cast<std::streamsize>(texture_data.size()));
                    outfile.close();

                    spdlog::info("Successfully processed texture of GLTF file '{}' and written it to '{}'",
                        input_file.string(),
                        outfile_path);
                }));
            context.task_scheduler.AddTaskSetToPipe(task.get());
        }
        context.task_scheduler.WaitforAll();
    }
    else
    {
        switch (gltf.error())
        {
        case GLTF_Error::File_Load_Failed:
            spdlog::error("GLTF file '{}' failed to load.", input_file.string());
            break;
        case GLTF_Error::Parse_Failed:
            spdlog::error("GLTF file '{}' failed to parse.", input_file.string());
            break;
        default:
            break;
        }
    }
}

void process_file(Asset_Bake_Context& context, const std::filesystem::path& input_file)
{
    if (input_file.extension() == ".gltf")
    {
        process_gltf(context, input_file);
    }
}

void process_files(Asset_Bake_Context& context)
{
    for (const auto& directory_entry : std::filesystem::recursive_directory_iterator(context.input_directory))
    {
        if (!std::filesystem::is_directory(directory_entry))
        {
            const auto& directory_path = directory_entry.path();
            process_file(context, directory_path);
        }
    }
}

}

int32_t main(const int32_t argc, char** argv) try
{
    TCLAP::CmdLine cmd("Asset baker", ' ', "0.1", true);
    TCLAP::ValueArg<std::string> input_directory_arg(
        "i",
        "input-dir",
        "Set input directory - assets are processed recursively inside this directory",
        true,
        "",
        "string");
    cmd.add(input_directory_arg);
    TCLAP::ValueArg<std::string> output_directory_arg(
        "o",
        "output-dir",
        "Set output directory - assets are stored inside this directory",
        true,
        "",
        "string");
    cmd.add(output_directory_arg);
    // TCLAP::SwitchArg use_cache_arg(
    //     "c",
    //     "use-cache",
    //     "If set, don't process resources that are already in 'output-dir'",
    //     false);
    // cmd.add(use_cache_arg);
    TCLAP::ValueArg<int32_t> log_level_arg(
        "l",
        "log-level",
        "Set log level. 0 is trace, 1 is debug, 2 is info, 3 is warn, 4 is error, 5 is critical.",
        false,
        2,
        "int");
    cmd.add(log_level_arg);
    cmd.parse(argc, argv);

    spdlog::set_level(static_cast<spdlog::level::level_enum>(log_level_arg.getValue()));

    asset_baker::Asset_Bake_Context asset_bake_context = {
        .input_directory = input_directory_arg.getValue(),
        .output_directory = output_directory_arg.getValue(),
    //     .use_cache = use_cache_arg.getValue()
        .task_scheduler = enki::TaskScheduler()
    };
    asset_bake_context.task_scheduler.Initialize();
    asset_baker::process_files(asset_bake_context);

    return 0;
}
catch (TCLAP::ArgException& e)
{
    spdlog::critical("Error: '{}' at '{}'", e.error(), e.argId());
    return -2;
}
catch (...)
{
    spdlog::critical("An unknown error occurred.");
    return -1;
}

/// YANG Development Kit
// Copyright 2016 Cisco Systems. All rights reserved
//
////////////////////////////////////////////////////////////////
// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.
//
//////////////////////////////////////////////////////////////////

#include <iostream>

#include "codec_provider.hpp"
#include "codec_service.hpp"
#include "logger.hpp"
#include "entity_data_node_walker.hpp"
#include "path_api.hpp"
#include "types.hpp"

namespace ydk
{

const char * REPO_ERROR_MSG ="Failed to initialize provider.";

const char * PAYLOAD_ERROR_MSG ="Codec service only supports one entity per payload, please split payload";

static augment_capabilities_function get_augment_capabilities_function(Entity & entity);
static std::string get_bundle_yang_models_location(Entity & entity);
static std::string get_bundle_name(Entity & entity);


CodecService::CodecService()
{
}

CodecService::~CodecService()
{
}

std::string
CodecService::encode(CodecServiceProvider & provider, Entity & entity, bool pretty)
{
    auto const & bundle_name = get_bundle_name(entity);
    provider.initialize(bundle_name, get_bundle_yang_models_location(entity), get_augment_capabilities_function(entity));

    path::RootSchemaNode& root_schema = provider.get_root_schema(bundle_name);
    try
    {
        path::DataNode& data_node = get_data_node_from_entity(entity, root_schema);
        path::CodecService core_codec_service{};
        std::string result = core_codec_service.encode(data_node, provider.m_encoding, pretty);
        YLOG_DEBUG("Performing encode operation, resulting in {}", result);
        return result;
    }
    catch (const YCPPInvalidArgumentError& e)
    {
        YLOG_ERROR(REPO_ERROR_MSG);
        throw(YCPPServiceProviderError(REPO_ERROR_MSG));
    }
    return {};
}

std::map<std::string, std::string>
CodecService::encode(CodecServiceProvider & provider, std::map<std::string, std::unique_ptr<Entity>> & entity_map, bool pretty)
{
    std::map<std::string, std::string> payload_map;
    for (auto it = entity_map.begin(); it != entity_map.end(); ++it)
    {
        std::string payload = encode(provider, *(it->second), pretty);
        payload_map[it->first] = payload;
    }
    return payload_map;
}

std::unique_ptr<Entity>
CodecService::decode(CodecServiceProvider & provider, std::string & payload, std::unique_ptr<Entity> entity)
{
    auto const & bundle_name = get_bundle_name(*entity);
    provider.initialize(bundle_name, get_bundle_yang_models_location(*entity), get_augment_capabilities_function(*entity));

    path::RootSchemaNode& root_schema = provider.get_root_schema(bundle_name);

    YLOG_DEBUG("Performing decode operation on {}", payload);

    path::CodecService core_codec_service{};
    auto root_data_node = core_codec_service.decode(root_schema, payload, provider.m_encoding);

    if (root_data_node->children().size() != 1)
    {
    	YLOG_ERROR(PAYLOAD_ERROR_MSG);
        throw(YCPPServiceProviderError(PAYLOAD_ERROR_MSG));
    }
    else
    {
        for (auto data_node: root_data_node->children())
        {
            get_entity_from_data_node(data_node.get(), entity.get());
        }
    }
    return entity;
}

std::map<std::string, std::unique_ptr<Entity>>
CodecService::decode(CodecServiceProvider & provider, std::map<std::string, std::string> & payload_map,
		std::map<std::string, std::unique_ptr<Entity>> entity_map)
{
    for (auto it: payload_map)
    {
        std::unique_ptr<Entity> entity = decode(provider, it.second, std::move(entity_map[it.first]));
        entity_map[it.first] = std::move(entity);
    }
    return entity_map;
}

static augment_capabilities_function get_augment_capabilities_function(Entity & entity)
{
    if(entity.parent == nullptr)
        return entity.get_augment_capabilities_function();

    return get_augment_capabilities_function(*(entity.parent));
}

static std::string get_bundle_yang_models_location(Entity & entity)
{
    if(entity.parent == nullptr)
        return entity.get_bundle_yang_models_location();

    return get_bundle_yang_models_location(*(entity.parent));
}

static std::string get_bundle_name(Entity & entity)
{
    if(entity.parent == nullptr)
        return entity.get_bundle_name();

    return get_bundle_name(*(entity.parent));
}

}

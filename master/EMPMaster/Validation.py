from EMPMaster.Errors import *

CE = InvalidServerConfigurationError
SE = InvalidServerStateError

isstr  = lambda x: isinstance(x, basestring)
islist = lambda x: isinstance(x, list)
isdict = lambda x: isinstance(x, dict)
isbool = lambda x: isinstance(x, bool)
isint  = lambda x: isinstance(x, int)

def validate_resources(resources):
    if not islist(resources):
        raise CE('"resources" section is not an array.')
    for resource in resources:
        if isdict(resource):
            if not 'name' in resource:
                raise CE('Unnamed resource in "resources" section.')
            if 'type' in resource:
                if resource['type'] not in ('iwad', 'wad', 'dehacked'):
                    raise CE(
                      'Resource\'s type is not in ("iwad", "wad", "dehacked")'
                    )
            if 'alternates' in resource:
                if not islist(resource['alternates']):
                    raise CD("Resource's alternates is not a list.")
                for alternate in resource['alternates']:
                    if not isstr(resource['alternates'][alternate]):
                        raise CD("Resource's alternate is not a string.")
        elif not isstr(resource):
            raise CE('Invalid data type in "resources" section.')

def validate_maps(maps):
    if not islist(maps):
        raise CE('"maps" section is not an array.')
    for map in maps:
        if isdict(map):
            if 'name' not in map:
                raise CE('Unnamed map in "maps" section.')
            if 'wads' in map:
                if not islist(map['wads']):
                    raise CE("Map's WADs is not an array.")
                for wad in map['wads']:
                    if not isstr(wad):
                        raise CE("Map's wad is not a string.")
            if 'overrides' in map:
                validate_options(map['overrides'], 'overrides')
        elif not isstr(map):
            raise CE('Invalid data type in "maps" section.')

def validate_server(server):
    if not isdict(server):
        raise CE('"server" section is not an object.')
    for name, s, validator in [
        ('address', 'a string', isstr),
        ('port', 'an integer', isint),
        ('max_clients', 'an integer', isint),
        ('game_type', 'a string', isstr),
        ('number_of_teams', 'an integer', isint)
    ]:
        if not validator(server[name]):
            raise CE('Server\'s "%s" is not %s.' % (name, s))

def validate_options(options, section_name='options'):
    if not islist(options):
        raise CE('"%s" section is not an array.' % (section_name))
    for name, s, validator in [
        ('max_players', 'an integer', isint),
        ('dogs', 'an integer', isint),
        ('skill', 'an integer', isint),
        ('frag_limit', 'an integer', isint),
        ('time_limit', 'an integer', isint),
        ('score_limit', 'an integer', isint),
        ('spawn_armor', 'a boolean', isbool),
        ('spawn_super_items', 'a boolean', isbool),
        ('respawn_health', 'a boolean', isbool),
        ('respawn_items', 'a boolean', isbool),
        ('respawn_armor', 'a boolean', isbool),
        ('respawn_super_items', 'a boolean', isbool),
        ('respawn_barrels', 'a boolean', isbool),
        ('spawn_monsters', 'a boolean', isbool),
        ('fast_monsters', 'a boolean', isbool),
        ('strong_monsters', 'a boolean', isbool),
        ('powerful_monsters', 'a boolean', isbool),
        ('respawn_monsters', 'a boolean', isbool),
        ('allow_exit', 'a boolean', isbool),
        ('kill_on_exit', 'a boolean', isbool),
        ('exit_to_same_level', 'a boolean', isbool),
        ('force_respawn_time_limit', 'an integer', isint),
        ('force_respawn_action', 'respawn', isstr),
        ('spawn_in_same_spot', 'a boolean', isbool),
        ('spawn_farthest', 'a boolean', isbool),
        ('respawn_protection_time', 'an integer', isint),
        ('leave_keys', 'a boolean', isbool),
        ('leave_weapons', 'a boolean', isbool),
        ('drop_weapons', 'a boolean', isbool),
        ('drop_items', 'a boolean', isbool),
        ('infinite_ammo', 'a boolean', isbool),
        ('keep_items_on_exit', 'a boolean', isbool),
        ('keep_keys_on_exit', 'a boolean', isbool),
        ('enable_variable_friction', 'a boolean', isbool),
        ('enable_boom_push_effects', 'a boolean', isbool),
        ('enable_nukage', 'a boolean', isbool),
        ('allow_jump', 'a boolean', isbool),
        ('allow_freelook', 'a boolean', isbool),
        ('allow_crosshair', 'a boolean', isbool),
        ('allow_movebob_change', 'a boolean', isbool),
        ('disable_silent_bfg', 'a boolean', isbool),
        ('allow_two_way_wallrun', 'a boolean', isbool),
        ('allow_no_weapon_switch_on_pickup', 'a boolean', isbool),
        ('allow_preferred_weapon_order', 'a boolean', isbool),
        ('allow_silent_weapon_pickup', 'a boolean', isbool),
        ('allow_weapon_recoil', 'a boolean', isbool),
        ('allow_weapon_speed_change', 'a boolean', isbool),
        ('teleport_missiles', 'a boolean', isbool),
        ('instagib', 'a boolean', isbool),
        ('allow_chasecam', 'a boolean', isbool),
        ('imperfect_god_mode', 'a boolean', isbool),
        ('time_limit_powerups', 'a boolean', isbool),
        ('normal_sky_when_invulnerable', 'a boolean', isbool),
        ('zombie_players_can_exit', 'a boolean', isbool),
        ('arch_viles_can_create_ghosts', 'a boolean', isbool),
        ('limit_lost_souls', 'a boolean', isbool),
        ('lost_souls_get_stuck_in_walls', 'a boolean', isbool),
        ('lost_souls_never_bounce_on_floors', 'a boolean', isbool),
        ('monsters_randomly_walk_off_lifts', 'a boolean', isbool),
        ('monsters_get_stuck_on_door_tracks', 'a boolean', isbool),
        ('monsters_give_up_pursuit', 'a boolean', isbool),
        ('actors_get_stuck_over_dropoffs', 'a boolean', isbool),
        ('actors_never_fall_off_ledges', 'a boolean', isbool),
        ('monsters_can_telefrag_on_map30', 'a boolean', isbool),
        ('monsters_can_respawn_outside_map', 'a boolean', isbool),
        ('disable_terrain_types', 'a boolean', isbool),
        ('disable_falling_damage', 'a boolean', isbool),
        ('actors_have_infinite_height', 'a boolean', isbool),
        ('doom_actor_heights_are_inaccurate', 'a boolean', isbool),
        ('bullets_never_hit_floors_and_ceilings', 'a boolean', isbool),
        ('respawns_are_sometimes_silent_in_dm', 'a boolean', isbool),
        ('turbo_doors_make_two_closing_sounds', 'a boolean', isbool),
        ('disable_tagged_door_light_fading', 'a boolean', isbool),
        ('use_doom_stairbuilding_method', 'a boolean', isbool),
        ('use_doom_floor_motion_behavior', 'a boolean', isbool),
        ('use_doom_linedef_trigger_model', 'a boolean', isbool),
        ('line_effects_work_on_sector_tag_zero', 'a boolean', isbool),
        ('one_time_line_effects_can_break', 'a boolean', isbool),
        ('monsters_remember_target', 'a boolean', isbool),
        ('monster_infighting', 'a boolean', isbool),
        ('monsters_back_out', 'a boolean', isbool),
        ('monsters_avoid_hazards', 'a boolean', isbool),
        ('monsters_affected_by_friction', 'a boolean', isbool),
        ('monsters_climb_tall_stairs', 'a boolean', isbool),
        ('friend_distance', 'an integer', isint),
        ('rescue_dying_friends', 'a boolean', isbool),
        ('dogs_can_jump_down', 'a boolean', isbool)
    ]:
        if name in options and not validator(options[name]):
            raise CE('%s "%s" is not %s.' % (
                option_name[:-1].capitalize(), name, s
            ))
        if 'force_respawn_action' in options:
            if options['force_respawn_action'] not in ('respawn', 'spectate'):
                es = ('%s "force_respawn_action is not in ("respawn", '
                      '"spectate").')
                raise CE(es % (option_name[:-1].capitalize()))

def validate_configuration(config):
    for section, validator in (
            ('resources', validate_resources),
            ('server', validate_server),
            ('options', validate_options),
            ('maps', validate_maps)
        ):
        if section not in config:
            raise InvalidServerConfigurationError('"%s" section missing.' % (
                section
            ))
            validator(section)

def validate_state(state):
    ###
    # [CG] Haven't figured the schema for this out yet, so I guess by
    #      definition everything is valid!  TODO!
    ###
    pass


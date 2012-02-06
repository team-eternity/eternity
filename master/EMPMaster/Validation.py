from EMPMaster.Errors import *

CE = InvalidServerConfigurationError
SE = InvalidServerStateError

isstr   = lambda x: x is None or isinstance(x, basestring)
islist  = lambda x: x is None or isinstance(x, list)
isdict  = lambda x: x is None or isinstance(x, dict)
isbool  = lambda x: x is None or isinstance(x, bool)
isint   = lambda x: x is None or isinstance(x, int)
isfloat = lambda x: x is None or isinstance(x, float)

validator_to_string = {
    isstr:   'a string',
    islist:  'a list',
    isdict:  'a dict',
    isbool:  'a boolean',
    isint:   'an integer',
    isfloat: 'a float'
}

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

def validate_server(server):
    if not isdict(server):
        raise CE('"server" section is not an object.')
    for name, validator in [
        ('address', isstr),
        ('game', isstr),
        ('game_type', isstr),
        ('hostname', isstr),
        ('max_clients', isint),
        ('port', isint),
        ('requires_player_password', isbool),
        ('requires_spectator_password', isbool),
        ('join_time_limit', isint),
        ('wad_repository', isstr)
    ]:
        if not validator(server[name]):
            raise CE('Server\'s "%s" is not %s.' % (
                name,
                validator_to_string(validator)
            ))

def validate_options(options, section_name='options'):
    if not isdict(options):
        raise CE('"%s" section is not an array.' % (section_name))
    for name, validator in [
        ('actors_get_stuck_over_dropoffs', isbool),
        ('actors_have_infinite_height', isbool),
        ('actors_never_fall_off_ledges', isbool),
        ('allow_chasecam', isbool),
        ('allow_crosshair', isbool),
        ('allow_damage_screen_change', isbool),
        ('allow_exit', isbool),
        ('allow_freelook', isbool),
        ('allow_jump', isbool),
        ('allow_movebob_change', isbool),
        ('allow_no_weapon_switch_on_pickup', isbool),
        ('allow_preferred_weapon_order', isbool),
        ('allow_target_names', isbool),
        ('allow_two_way_wallrun', isbool),
        ('allow_weapon_speed_change', isbool),
        ('arch_viles_can_create_ghosts', isbool),
        ('bfg_type', isstr),
        ('build_blockmap', isbool),
        ('bullets_never_hit_floors_and_ceilings', isbool),
        ('death_time_expired_action', isstr),
        ('death_time_limit', isint),
        ('disable_falling_damage', isbool),
        ('disable_tagged_door_light_fading', isbool),
        ('disable_terrain_types', isbool),
        ('dogs', isint),
        ('dogs_can_jump_down', isbool),
        ('doom_actor_heights_are_inaccurate', isbool),
        ('enable_boom_push_effects', isbool),
        ('enable_nukage', isbool),
        ('enable_variable_friction', isbool),
        ('exit_to_same_level', isbool),
        ('fast_monsters', isbool),
        ('follow_fragger_on_death', isbool),
        ('frag_limit', isint),
        ('friend_distance', isint),
        ('friendly_damage_percentage', isint),
        ('imperfect_god_mode', isbool),
        ('infinite_ammo', isbool),
        ('instagib', isbool),
        ('keep_items_on_exit', isbool),
        ('keep_keys_on_exit', isbool),
        ('kill_on_exit', isbool),
        ('leave_keys', isbool),
        ('leave_weapons', isbool),
        ('limit_lost_souls', isbool),
        ('line_effects_work_on_sector_tag_zero', isbool),
        ('lost_souls_get_stuck_in_walls', isbool),
        ('lost_souls_never_bounce_on_floors', isbool),
        ('max_players', isint),
        ('max_players_per_team', isint),
        ('monster_infighting', isbool),
        ('monsters_affected_by_friction', isbool),
        ('monsters_avoid_hazards', isbool),
        ('monsters_back_out', isbool),
        ('monsters_can_respawn_outside_map', isbool),
        ('monsters_can_telefrag_on_map30', isbool),
        ('monsters_climb_tall_stairs', isbool),
        ('monsters_get_stuck_on_door_tracks', isbool),
        ('monsters_give_up_pursuit', isbool),
        ('monsters_randomly_walk_off_lifts', isbool),
        ('monsters_remember_target', isbool),
        ('normal_sky_when_invulnerable', isbool),
        ('one_time_line_effects_can_break', isbool),
        ('players_drop_everything', isbool),
        ('players_drop_items', isbool),
        ('players_drop_weapons', isbool),
        ('powerful_monsters', isbool),
        ('radial_attack_damage', isfloat),
        ('radial_attack_lift', isfloat),
        ('radial_attack_self_damage', isfloat),
        ('radial_attack_self_lift', isfloat),
        ('radius_attacks_only_thrust_in_2d', isbool),
        ('rescue_dying_friends', isbool),
        ('respawn_armor', isbool),
        ('respawn_barrels', isbool),
        ('respawn_health', isbool),
        ('respawn_items', isbool),
        ('respawn_monsters', isbool),
        ('respawn_protection_time', isint),
        ('respawn_super_items', isbool),
        ('respawns_are_sometimes_silent_in_dm', isbool),
        ('score_limit', isint),
        ('short_vertical_mouselook_range', isbool),
        ('silent_weapon_pickup', isbool),
        ('skill', isint),
        ('spawn_armor', isbool),
        ('spawn_farthest', isbool),
        ('spawn_in_same_spot', isbool),
        ('spawn_monsters', isbool),
        ('spawn_super_items', isbool),
        ('strong_monsters', isbool),
        ('teleport_missiles', isbool),
        ('time_limit', isint),
        ('time_limit_powerups', isbool),
        ('turbo_doors_make_two_closing_sounds', isbool),
        ('use_doom_floor_motion_behavior', isbool),
        ('use_doom_linedef_trigger_model', isbool),
        ('use_doom_stairbuilding_method', isbool),
        ('use_oldschool_sound_cutoff', isbool),
        ('zombie_players_can_exit', isbool)
    ]:
        if name in options and not validator(options[name]):
            raise CE('%s "%s" is not %s.' % (
                option_name[:-1].capitalize(),
                name,
                validator_to_string(validator)
            ))
    if 'death_time_expired_action' in options and \
        options['death_time_expired_action'] not in ('respawn', 'spectate'):
        es = '%s "death_time_expired_action is not in ("respawn", "spectate").'
        raise CE(es % (option_name[:-1].capitalize()))


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
        validator(config[section])

def validate_state(state):
    ###
    # [CG] Haven't figured the schema for this out yet, so I guess by
    #      definition everything is valid!  TODO!
    ###
    pass


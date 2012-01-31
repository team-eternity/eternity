/*
 * [CG] TODO:
 *   - Store server passwords using HTML5 Storage
 *   - Current server settings
 *     - Configuration
 *     - Full IWAD & PWAD listing
 *       - If the server provided a WAD repository link, list PWADs as links
 */
var max_hostname_length = 38;
var max_pwads_length = 21;

var servers = null;
var username = null;
var password_hash = null;
var server_groups = null;
var email_regexp = /^((([a-z]|\d|[!#\$%&'\*\+\-\/=\?\^_`{\|}~]|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])+(\.([a-z]|\d|[!#\$%&'\*\+\-\/=\?\^_`{\|}~]|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])+)*)|((\x22)((((\x20|\x09)*(\x0d\x0a))?(\x20|\x09)+)?(([\x01-\x08\x0b\x0c\x0e-\x1f\x7f]|\x21|[\x23-\x5b]|[\x5d-\x7e]|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])|(\\([\x01-\x09\x0b\x0c\x0d-\x7f]|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF]))))*(((\x20|\x09)*(\x0d\x0a))?(\x20|\x09)+)?(\x22)))@((([a-z]|\d|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])|(([a-z]|\d|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])([a-z]|\d|-|\.|_|~|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])*([a-z]|\d|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])))\.)+(([a-z]|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])|(([a-z]|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])([a-z]|\d|-|\.|_|~|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])*([a-z]|[\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF])))\.?$/i; // [CG] BAHAHAHAHAHAHAHAHA.
var tablesorter_initialized = false;

function escapeHTML(s) {
    return $('<div/>').text(s).html();
}

function validateEMail(address) {
    if (email_regexp.test(address)) {
        return true;
    }
    return false;
}

function wrapForm(form_id, submit_text, submit_func) {
    var img =
        '<img class="loading" src="<static path="/images/loading.gif"/>"/>';
    var button = $('<button></button>').text(submit_text).click(submit_func);
    $('#' + form_id + ' > input').keypress(function (e) {
        var keychar;
        if (window.event) { // Old IE
            keychar = e.keyCode;
        }
        else if (e.which) { // Something else
            keychar = e.which;
        }
        if (keychar == 13) {
            submit_func();
        }
    });
    $('#' + form_id).append(img).append(button);
}

function ellipsize(s, length) {
    return s;
    if (length == null) {
        length = 30;
    }
    if (s.length > length) {
        return s.substr(0, length - 3) + '...';
    }
    return s;
}

function formatList(l, length) {
    if ((!l) || (l.length == 0)) {
        return "";
    }
    return ellipsize(l.join(', '), length);
}

function formatPWADs(pwads) {
    return String(formatList(pwads)).replace(/\.wad/g, '');
}

function formatTime(ss) {
    t = parseInt(ss);
    hours = parseInt(t / 3600);
    if (hours >= 1) {
        return '59:59';
    }
    minutes = parseInt(t / 60);
    if (minutes < 1) {
        minutes = '00';
    }
    else if (minutes < 10) {
        minutes = '0' + minutes;
    }
    seconds = parseInt(t % 60);
    if (seconds < 1) {
        seconds = '00';
    }
    else if (seconds < 10) {
        seconds = '0' + seconds;
    }
    return minutes + ':' + seconds;
}

function formatGameType(game_type) {
    if (game_type == 'deathmatch') {
        return 'DM';
    }
    else if (game_type == 'tdm') {
        return 'TDM';
    }
    else if (game_type == 'ctf') {
        return 'CTF';
    }
    else if (game_type == 'duel') {
        return 'DUEL';
    }
    else if (game_type == 'cooperative' || game_type == 'coop') {
        return 'COOP';
    }
    else {
        return '-';
    }
}

function setProtocolToEternity(uri) {
    var x = new String(uri);
    var colon_index = x.indexOf(":");
    if (colon_index == -1) {
        return x; // [CG] URI is probably invalid, so screw it.
    }
    return 'eternity' + x.substr(colon_index);
};

function toURI(group_name, server_name) {
    return setProtocolToEternity(window.location) +
           'servers/' +
           encodeURIComponent(group_name) +
           '/' +
           encodeURIComponent(server_name);
}

function toLink(group_name, server_name, hostname) {
    var uri = toURI(group_name, server_name);
    hostname = ellipsize(hostname, max_hostname_length);
    return '<a class="server_link" href="' + uri + '">' + hostname + '</a>';
}

function buildServerGroups() {
    var server_groups_element = $('#server_groups');
    server_groups_element.empty();
    $.each(server_groups, function (index, g) {
        var d = '<div class="server_group">' + g + '</div>';
        server_groups_element.append(d);
    });
    $('.server_group').click(function () {
        $(this).toggleClass("delete_this_group");
    });
    if (server_groups.length > 0) {
        server_groups_element.append(
            '<button onclick="sendDeleteServerGroups()">Delete</button>'
        ).append('<hr/>');
    }
}

function clearForm(form_id) {
    $('.error').remove();
    $('.success').remove();
    $('#' + form_id + ' img.loading').show();
}

function setFormSuccess(form_id, text) {
    var loading_image = $('#' + form_id + ' img.loading');
    if (text != null && text.length > 0) {
        loading_image.hide().after('<span class="success">' + text + '</span>');
    }
    else {
        loading_image.hide();
    }
}

function setFormError(form_id, text) {
    var loading_image = $('#' + form_id + ' img.loading');
    if (text != null && text.length > 0) {
        loading_image.hide().after('<span class="error">' + text + '</span>');
    }
    else {
        loading_image.hide();
    }
}

/*
 * [CG] Browsers are supposed to transparently follow 301 redirects, but only
 *      for GET requests.  Unfortunately most do this for all methods, which
 *      they are explicitly not supposed to do.  As a result, jQuery will
 *      receive all sorts of incorrect status codes when something already
 *      exists, including what it should receive (301).  This is why some of
 *      these error handlers check for codes outside of 301, but still report
 *      that the resource already exists.
 */

function sendRegistration() {
    clearForm('registration_form');
    var username_field_value = $('#reg_name').val();
    var password_field_value = $('#reg_password').val();
    var email_field_value = $('#reg_email').val();
    if (username_field_value.length < 1 ||
        password_field_value.length < 1 ||
        email_field_value.length < 1) {
        setFormError('registration_form', 'All fields required.');
        return;
    }
    if (!validateEMail(email_field_value)) {
        setFormError('registration_form', 'Invalid e-mail address.');
        return;
    }
    $.ajax({
        type: 'PUT',
        url: window.location + 'users/' + username_field_value,
        data: $.param({
            username: Base64.encode(username_field_value),
            password: Base64.encode(Sha1.hash(password_field_value)),
            email: Base64.encode(email_field_value)
        }),
        success: function (data, text_status, xhr) {
            setFormSuccess('registration_form');
            var msg = 'Registration successful, please check your e-mail ' +
                      'for verification instructions, and then login ';
            setAccountFormSwitcher(msg, 'here.', showLoginForm);
        },
        error: function (xhr, status, error) {
            if (xhr.status == 301 || xhr.status == 401) {
                var error = 'User already exists.';
            }
            else {
                var error = xhr.responseText;
            }
            if (error == 'None') {
                error = 'Internal Server Error';
            }
            setFormError('registration_form', error);
        }
    });
}

function sendLogin() {
    clearForm('login_form');
    var username_field_value = $('#login_name').val();
    var url = window.location + 'users';
    username = username_field_value;
    password_hash = Sha1.hash($('#login_password').val());
    $.ajax({
        type: 'HEAD',
        url: url,
        data: {
            username: Base64.encode(username),
            password: Base64.encode(password_hash)
        },
        success: sendGetServerGroups,
        error: function (xhr, status, error) {
            username = null;
            password_hash = null;
            if (xhr.status == 401) {
                var error = 'Login failed.';
            }
            else {
                var error = xhr.responseText;
                if (error == 'None') {
                    error = 'Internal Server Error';
                }
            }
            setFormError('login_form', error);
        }
    });
}

function setAuthHeader(xhr) {
    xhr.setRequestHeader(
        'Authorization',
        'Basic ' + Base64.encode(username + ':' + password_hash)
    );
}

function sendGetServerGroups() {
    clearForm('login_form');
    var username_field_value = $('#login_name').val();
    var url = window.location + 'users/' + username_field_value;
    $.ajax({
        type: 'GET',
        url: url,
        dataType: 'json',
        beforeSend: setAuthHeader,
        success: function (groups, status, request) {
            setFormSuccess('login_form');
            server_groups = groups;
            buildServerGroups();
            showServerGroupsForm();
        },
        error: function (xhr, status, error) {
            error = xhr.responseText;
            if (error == 'None') {
                error = 'Internal Server Error';
            }
            setFormError('login_form', error);
        }
    });
}

function sendDeleteServerGroups() {
    var error = false;
    var removed_count = 0;
    clearForm('server_groups_form');
    $('.delete_this_group').each(function (index) {
        var element = $(this);
        var text = element.text();
        $.ajax({
            async: false, // [CG] Do this synchronously.
            type: 'DELETE',
            url: window.location + 'servers/' + text,
            beforeSend: setAuthHeader,
            error: function (xhr, status, err) {
                error = xhr.responseText;
                if (error == 'None') {
                    error = 'Internal Server Error';
                }
                setFormError('login_form', error);
            },
            success: function (data, text_status, xhr) {
                removed_count++;
                error = false;
                element.remove();
            }
        });
        if (error != false) {
            return false; // [CG] Should break out of the .each loop.
        }
    });
    if (error != false) {
        setFormError('server_groups_form', error);
    }
    else if (removed_count < 1) {
        setFormSuccess('server_groups_form');
    }
    else {
        $.ajax({
            async: false, // [CG] Do this synchronously.
            url: window.location + 'users/' + Base64.decode(username),
            dataType: 'json',
            beforeSend: setAuthHeader,
            error: function (xhr, status, err) {
                if (xhr.responseText == 'None') {
                    setFormError('server_groups_form', 'Internal Server Error');
                }
                else {
                    setFormError('server_groups_form', xhr.responseText);
                }
            },
            success: function (groups, status, request) {
                server_groups = groups;
                buildServerGroups();
                setFormSuccess('server_groups_form', 'Group(s) removed.');
            },
        });
    }
}

function sendNewServerGroup() {
    clearForm('server_groups_form');
    var new_group_name = $('#new_group_name').val();
    $.ajax({
        type: 'PUT',
        url: window.location + 'servers/' + new_group_name,
        beforeSend: setAuthHeader,
        error: function (xhr, status, error) {
            if (xhr.status == 301 || xhr.status == 200) {
                var error = 'Group already exists.';
            }
            else {
                var error = xhr.responseText;
            }
            if (error == 'None') {
                error = 'Internal Server Error';
            }
            setFormError('server_groups_form', error);
        },
        success: function (data, text_status, xhr) {
            var url = window.location + 'users/' + Base64.decode(username);
            server_groups.push(new_group_name);
            buildServerGroups();
            $('#new_group_name').val('');
            setFormSuccess('server_groups_form', 'Group added.');
        }
    });
}

function sendNewPassword() {
    clearForm('change_password_form');
    if (username == null || password_hash == null) {
        setFormError('change_password_form', 'Not logged in.');
        return;
    }
    var old_password_hash = Sha1.hash($('#current_password').val());
    var new_password_one = $('#new_password_1').val();
    var new_password_two = $('#new_password_2').val();
    if (new_password_one != new_password_two) {
        setFormError('change_password_form', "Passwords don't match.");
        return;
    }
    if (old_password_hash != password_hash) {
        setFormError('change_password_form', 'Invalid password.');
        return;
    }
    var new_password_hash = Sha1.hash(new_password_one);
    $.ajax({
        type: 'POST',
        url: window.location + 'users/' + username,
        beforeSend: setAuthHeader,
        data: { new_password: Base64.encode(new_password_hash) },
        error: function (xhr, status, error) {
            var error = xhr.responseText;
            if (error == 'None') {
                error = 'Internal Server Error';
            }
            setFormError('change_password_form', error);
        },
        success: function (data, text_status, xhr) {
            password_hash = new_password_hash;
            setFormSuccess('change_password_form', 'Password changed.');
        }
    });
}

function rowSelected(row_id) {
    highlightRow(row_id);
    showServerInfo(row_id);
}

function highlightRow(row_id) {
    $('#servers_table_body').children('tr').removeClass('highlighted');
    $('#server_row_' + row_id.toString()).addClass('highlighted');
}

function getRowColorClass(color, playing) {
    if (color == "red") {
        var color_class = "red_player_row";
    }
    else if (color == "blue") {
        var color_class = "blue_player_row";
    }
    else if (color == "green") {
        var color_class = "green_player_row";
    }
    else if (color == "white") {
        var color_class = "white_player_row";
    }
    else if (color == "black") {
        var color_class = "black_player_row";
    }
    else if (color == "purple") {
        var color_class = "purple_player_row";
    }
    else if (color == "yellow") {
        var color_class = "yellow_player_row";
    }
    else {
        var color_class = "no_team_player_row";
    }
    if (playing) {
        return color_class;
    }
    return color_class + ' spectating_player_row';
}

function buildTeamsTable(teams, parent_element) {
    var teams_table = $('<table id="teams_table">');
    var teams_table_body = $('<tbody id="teams_table_body">');
    $('<thead>' +
          '<tr>' +
              '<th>Player</th>' +
              '<th>Lag / PL</th>' +
              '<th>Frags</th>' +
              '<th>Time</th>' +
          '</tr>' +
      '</thead>'
     ).appendTo(teams_table);
    var team_row_index;
    var found_a_player = false;
    for (team_row_index = 0; team_row_index < teams.length; team_row_index++) {
        var team_row = teams[team_row_index];

        if (!team_row.players) {
           continue;
        }

        found_a_player = true;

        $.each(team_row.players, function (index, row) {
            if (row) {
                var row_color_class = getRowColorClass(
                    team_row.color, row.playing
                );
                var lag = row.lag + 'ms';
                var pl = row.packet_loss + '%';
                var tm = formatTime(row.time);
                $('<tr class="' + row_color_class + '">' +
                      '<td class="name_cell">' + row.name + '</td>' +
                      '<td class="lag_cell">' + lag + ' / ' + pl + '</td>' +
                      '<td class="frags_cell">' + row.frags + '</td>' +
                      '<td class="time_cell">' + tm + '</td>' +
                  '</tr>'
                ).appendTo(teams_table_body);
            }
        });
    }
    if (found_a_player) {
        teams_table_body.appendTo(teams_table);
        teams_table.appendTo(parent_element);
    }
    else {
        $('#sidebar').append('<div class="centered">- No Players -</div>');
    }
}

function buildPlayersTable(players, parent_element) {
    $('<table id="players_table"></table>').appendTo(parent_element);
    $('<thead>' +
          '<tr>' +
              '<th>Player</th>' +
              '<th colspan="2">Lag / PL</th>' +
              '<th>Frags</th>' +
              '<th>Time</th>' +
          '</tr>' +
      '</thead>' +
      '<tbody id="players_table_body"></tbody>'
    ).appendTo('#players_table');
    $.each(players, function (index, row) {
        if (row) {
            var lag = row.lag + 'ms';
            var pl = row.packet_loss + '%';
            var tm = formatTime(row.time);
            if (row.playing) {
                var row_class = "no_team_player_row";
            }
            else {
                var row_class = "no_team_player_row spectating_player_row";
            }
            $('<tr class="' + row_class + '">' +
                  '<td class="name_cell">' + row.name + '</td>' +
                  '<td class="lag_cell">' + lag + ' / ' + pl + '</td>' +
                  '<td class="frags_cell">' + row.frags + '</td>' +
                  '<td class="time_cell">' + tm + '</td>' +
              '</tr>'
            ).appendTo('#players_table_body');
        }
    });
}

function showServerInfo(server_index) {
    var data = servers[server_index];
    var config = data.configuration;
    var state = data.state;
    var repo = config.server.wad_repository;
    var sidebar = $('#sidebar');
    sidebar.empty().append('<h2 id="server_info_header">').append('<hr/>');
    $('#server_info_header').text(config.server.hostname);
    if (state.teams) {
        buildTeamsTable(state.teams, sidebar);
    }
    else if (state.players && state.players.length > 0) {
        buildPlayersTable(state.players, sidebar);
    }
    else {
        sidebar.append('<div class="centered">- No Players -</div>');
    }
    
    if (config.server.pwads.length > 0) {
        var pwads = $('<ul id="server_pwads_list">');
        var pwad;
        for (var i = 0; i < config.server.pwads.length; i++) {
            var name = config.server.pwads[i];
            if (repo) {
                var link = repo + name;
                $('<li>').addClass('server_pwad').append(
                   $('<a>').addClass('pwad_link').attr('href', link).html(name)
                ).appendTo(pwads);
            }
            else {
                $('<li>').addClass('server_pwad').html(name).appendTo(pwads);
            }
        }
        sidebar.append('<hr/>')
               .append('<h2 id="server_pwads_header">- Download WADs -</h2>')
               .append('<hr/>')
               .append(pwads);
    }
}

function addServer(index, server) {
    var config = server.configuration;
    var state = server.state;
    var link = toLink(
        config.server.group,
        config.server.name,
        config.server.hostname
    );
    var repo = config.server.wad_repository;
    config.server.pwads = new Array();
    $.each(config.resources, function(pindex, prow) {
        if (typeof(prow) == 'string') {
            config.server.pwads.push(prow);
        }
        else if (prow.type == 'wad' || prow.type == 'pwad') {
            config.server.pwads.push(prow.name);
        }
    });
    if (repo && repo.charAt(repo.length - 1) != '/') {
        config.server.wad_repository += '/';
    }
    var pwads = formatPWADs(
        config.server.pwads.slice(0, 2), max_pwads_length, null
    );
    if (config.server.pwads.length > 2) {
        pwads += ', ...';
    }
    var full_address = config.server.address + ':' +
        new String(config.server.port);
    var row_id = "'server_row_" + index + "'";
    var id_str = index.toString();
    $('<tr id=' + row_id + ' onclick="rowSelected(' + id_str + ')">' +
          '<td class="link_cell">' + link + '</td>' +
          '<td class="players_cell">' +
           state.connected_clients + ' / ' + config.server.max_player_clients +
          '</td>' +
          '<td class="game_type_cell">' +
              formatGameType(config.server.game_type) +
          '</td>' +
          '<td class="pwads_cell">' + pwads + '</td>' +
          '<td class="maps_cell">' + state.map + '</td>' +
      '</tr>'
     ).appendTo('#servers_table_body');
}

function rebuildServerTable() {
    $('#servers_table').replaceWith(
        '<table id="servers_table">' +
            '<thead id="servers_table_head">' +
                '<tr>' +
                    '<th>Server</th>' +
                    '<th>Clients</th>' +
                    '<th>Type</th>' +
                    '<th>WADs</th>' +
                    '<th>Map</th>' +
                '</tr>' +
            '</thead>' +
            '<tbody id="servers_table_body"></tbody>' +
        '</table>'
    );
}

function buildServerList(data, textStatus, xhr) {
    var sl = [[0, 0]];
    rebuildServerTable();
    if (data != null || servers != null) {
        servers = data;
        $.each(servers, addServer);
        $('#servers_table').tablesorter({
            sortMultiSortKey: "ctrlKey",
            sortList: [[0, 0]]
        });
        reInitializeScrollpane();
    }
    showServerList();
}

function reInitializeScrollpane() {
    var scrollpane_api = $('#main_window').data('jsp');
    if (scrollpane_api != null) {
        scrollpane_api.reinitialise();
    }
}

function clearMainWindow() {
    $('#servers_table').hide();
    $('#no_servers').hide();
    $('#sidebar').empty();
    $('#account_forms').hide();
    $('#login_form').hide();
    $('#registration_form').hide();
    $('#change_password_form').hide();
    $('#server_groups_form').hide();
    $('#account_form_switch').hide();
    $('#switcher').hide();
}

function setAccountFormSwitcher(text, link_text, link_func) {
    $('#account_form_switch').html(text + '<span id="switcher"></span>');
    $('#switcher').text(link_text).click(link_func);
}

function showAccountForms() {
    current_main_window_contents = "account_forms";
    if (username == null || password == null || server_groups == null) {
        showLoginForm();
    }
    else {
        showServerGroupsForm();
    }
    reInitializeScrollpane();
}

var current_main_window_contents = "server_list";

function switchMainWindow() {
    clearMainWindow();
    if (current_main_window_contents == "server_list") {
        showAccountForms();
        $('#section_label').html('< Servers');
        $('#refresh').unbind();
        $('#refresh_button').removeClass('active_top_button');

    }
    else {
        showServerList();
        $('#section_label').html('Account >');
        $('#refresh').click(loadServers);
        $('#refresh_button').addClass('active_top_button');
    }
}

function showServerList() {
    current_main_window_contents = "server_list";
    username = null;
    password_hash = null;
    $('#login_password').val('');
    if (servers != null && servers.length > 0) {
        $('#servers_table').show();
        rowSelected(0);
    }
    else {
        $('#no_servers').show();
    }
    reInitializeScrollpane();
}

function showAccountForm(switcher_text, switcher_link_text, switcher_link_func,
                         account_form_element) {
    clearMainWindow();
    setAccountFormSwitcher(
        switcher_text,
        switcher_link_text,
        switcher_link_func
    );
    $('#switcher').show();
    $('#account_form_switch').show();
    account_form_element.show();
    $('#account_forms').show();
}

function showLoginForm() {
    showAccountForm(
        "Don't have an account?  ",
        'Register.',
        showRegistrationForm,
        $('#login_form')
    );
}

function showRegistrationForm() {
    showAccountForm(
        'Already have an account?  ',
        'Login.',
        showLoginForm,
        $('#registration_form')
    );
}

function showChangePasswordForm() {
    showAccountForm(
        'Manage your server groups ',
        'here.',
        showServerGroupsForm,
        $('#change_password_form')
    );
}

function showServerGroupsForm() {
    showAccountForm(
        'Change your password ',
        'here.',
        showChangePasswordForm,
        $('#server_groups_form')
    );
}

function loadServers() {
    if (servers) {
        $('#servers_table_body').children().remove();
        servers = null;
    }
    $.getJSON(window.location + 'servers', buildServerList);
}

$(document).ready(function() {
    $('#main_window').jScrollPane();
    clearMainWindow();
    wrapForm('login_form', 'Login', sendLogin);
    wrapForm('registration_form', 'Register', sendRegistration);
    wrapForm('change_password_form', 'Set', sendNewPassword);
    wrapForm('server_groups_form', 'Create', sendNewServerGroup);
    $('#refresh').click(loadServers);
    $('#account').click(switchMainWindow);
    loadServers();
});

// vi:sw=4 ts=4:


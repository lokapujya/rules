<?php

require_once drupal_get_path('module', 'rules') . '/rules.admin_rule_proxy.inc';


/**
 * Lists the available rules.
 */
function rules_admin_form_overview(&$form_state) {
  $form['filter'] = array(
    '#type' => 'fieldset',
    '#title' => t('Filter'),
    '#collapsible' => TRUE,
    '#collapsed' => isset($form_state['values']),
  );
  $form['filter']['set'] = array(
    '#type' => 'select',
    '#title' => t('Filter by event'),
    '#options' => array(0 => '<All>') + rules_get_grouped_labels(rules_get_events()),
    '#default_value' => isset($form_state['values']['set']) ? $form_state['values']['set'] : 0,
  );
  $form['filter']['category'] = array(
    '#type' => 'select',
    '#title' => t('Filter by category'),
    '#options' => array(0 => '<All>') + rules_admin_get_categories(),
    '#default_value' => isset($form_state['values']['category']) ? $form_state['values']['category'] : 0,
  );
  $form['filter']['submit'] = array('#type' => 'submit', '#value' => t('Filter'));

  $category = isset($form_state['values']['category']) ? $form_state['values']['category'] : FALSE;
  $set = isset($form_state['values']['set']) ? 'event_'. $form_state['values']['set'] : FALSE;

  $form['active_header'] = array('#value' => '<h3>'. t('Active rules'). '</h3>');
  $form['active'] = rules_admin_overview_table(array('category' => $category, 'set' => $set, 'active' => TRUE));
  $form['active']['#suffix'] = '<br />';
  $form['inactive_header'] = array('#value' => '<h3>'. t('Inactive rules'). '</h3>');
  $form['inactive'] = rules_admin_overview_table(array('category' => $category, 'set' => $set, 'active' => FALSE));

  if (variable_get('rules_show_fixed', FALSE)) {
    $form['fixed_header'] = array('#value' => '<h3>'. t('Fixed rules'). '</h3>');
    $form['fixed'] = rules_admin_overview_table(array('category' => $category, 'set' => $set, 'active' => TRUE, 'fixed' => TRUE));
  }
  return $form;
}

function rules_admin_form_overview_submit($form_id, &$form_state) {
  $form_state['rebuild'] = TRUE;
}

/**
 * Returns a table of rules filtered by the given parameters
 * 
 * Supported parameters: fixed, category, set, active.
 * Category and set may be set to NULL to hide their columns.
 */
function rules_admin_overview_table($params) {
  $rules = rules_get_configured_items('rules');
  _rules_sort_children($rules);
  
  //set parameter defaults
  $params += array('category' => NULL, 'set' => NULL, 'active' => TRUE, 'fixed' => FALSE);
  extract($params);

  $header = array(t('Label'), t('Event'), t('Category'), t('Status'), t('Operations'));
  $rows = array();

  foreach (element_children($rules) as $name) {
    $rule = $rules[$name];
    _rules_element_defaults($rule);

    if ((!$category || in_array($category, $rule['#categories'])) && (!$set && strpos($rule['#set'], 'event_') === 0 || $rule['#set'] == $set) && $rule['#active'] == $active && ($rule['#status'] == 'fixed') == $fixed) {      
      $path = RULES_ADMIN_RULE_PATH .'/'. $name;
      $ops = array();
      if ($rule['#status'] != 'fixed') {
        $ops[] = l(t('edit'), $path .'/edit');
      }
      if ($rule['#status'] == 'custom') {
        $ops[] = l(t('delete'), $path .'/delete', array('query' => drupal_get_destination()));
      }
      else if ($rule['#status'] == 'altered') {
        $ops[] = l(t('reset'), $path .'/revert', array('query' => drupal_get_destination()));
      }
      $ops[] = l(t('clone'), $path .'/clone', array(), drupal_get_destination());
      $categories = array_map('check_plain', $rule['#categories']);

      $rows[] = array(
        l($rule['#label'], $path .'/edit'),
        check_plain(rules_get_element_label(rules_get_rule_sets($rule['#set']))),
        implode(', ', $categories),
        rules_admin_configuration_status($rule),
        implode(' ', $ops),
      );
    }
  }

  if (count($rows)) {
    return array('#value' => theme('table', $header, $rows, array('class' => 'rules-ng-configurations')));
  }

  return array('#value' => t('None.'));
}

/**
 * Returns the status of the configuration as html.
 */
function rules_admin_configuration_status($rule) {
  if ($rule['#status'] == 'altered') {
    $help = t('This rule has been provided by a module, but has been modified.');
    return '<span title="'. $help .'">'. t('Modified'). '</span>';
  }
  else if ($rule['#status'] == 'fixed') {
    $help = t('This rule has been provided by a module and can\'t be customized.');
    return '<span title="'. $help .'">'. t('Fixed') .'</span>';
  }
  else if ($rule['#status'] == 'custom') {
    $help = t('A custom defined rule.');
    return '<span title="'. $help .'">'. t('Custom'). '</span>';
  }
  $help = t('This rule has been provided by a module.');
  return '<span title="'. $help .'">'. t('Default') .'</span>';
}

/**
 * Returns the add rule form
 */
function rules_admin_form_configuration_add(&$form_state) {
  $form_state['set'] = arg(2) != 'trigger';
  return rules_admin_form_rule_settings(array(), $form_state['set']);
}

function rules_admin_form_configuration_add_submit($form_id, &$form_state) {
  $rule = isset($form_state['proxy']) ? $form_state['proxy']->get_rule() : array('#type' => 'rule');

  foreach (array('set', 'label', 'active', 'weight') as $key) {
    $rule['#'. $key] = $form_state['values'][$key];
  }
  $rule['#status'] = 'custom';

  //get an unique name
  $result = db_query("SELECT COUNT(name) FROM {rules_rules} WHERE name LIKE 'rules_%%'");
  $rule['#name'] = 'rules_'. (db_result($result) + 1);

  rules_item_save('rules', $rule['#name'], $rule);
  rules_clear_cache();
  drupal_set_message(t("The rule %label has been added.", array('%label' => $rule['#label'])));
  $form_state['redirect'] =  RULES_ADMIN_RULE_PATH .'/'. $rule['#name'];
}

/**
 * Returns the form for the settings of a rule
 */
function rules_admin_form_rule_settings($rule, $set = FALSE) {
  $form['settings'] = array(
    '#type' => 'fieldset',
    '#title' => t('Rule settings'),
    '#collapsible' => TRUE,
  );
  $form['settings']['label'] = array(
    '#title' => t('Label'),
    '#type' => 'textfield',
    '#description' => t('Choose an appropriate label for this rule.'),
    '#default_value' => isset($rule['#label']) ? $rule['#label'] : '',
    '#required' => TRUE,
  );
  $form['settings']['set'] = array(
    '#type' => 'select',
    '#default_value' => isset($rule['#set']) ? $rule['#set'] : '',
    '#required' => TRUE,
  );
  if ($set) {
    $form['settings']['set'] += array(
      '#title' =>t('Rule set'),
    	'#options' => rules_get_grouped_labels(rules_admin_get_compatible_sets($rule, rules_get_configured_items('rule_sets'))),
    	'#description' => t('Select to which rule set this rule should belong.'),
    );
  }
  else {
    $form['settings']['set'] += array(
      '#title' => t('Event'),
    	'#options' => rules_get_grouped_labels(rules_admin_get_compatible_sets($rule, rules_get_event_sets())),
    	'#description' => t('Select the event on which you want to evaluate this rule.'),
    );
  };
  $form['settings']['active'] = array(
    '#title' => t('This rule is active and should be evaluated when the associated event occurs.'),
    '#type' => 'checkbox',
    '#default_value' => isset($rule['#active']) ? $rule['#active'] : 1,
  );
  $form['settings']['weight'] = array(
    '#title' => t('Weight'),
    '#type' => 'weight',
    '#description' => t('Adjust the weight to customize the ordering of rules.'),
    '#default_value' => isset($rule['#weight']) ? $rule['#weight'] : 0,
  );
  $form['settings']['button'] = array('#type' => 'submit', '#weight' => 10, '#value' => t('Save changes'));
  return $form;
}

/**
 * Returns the form for editing a rule
 */
function rules_admin_form_edit_rule(&$form_state, $proxy) {

  $form_state['proxy'] = &$proxy;
  $rule = $proxy->get_rule();
  _rules_element_defaults($rule);

  $form = rules_admin_form_rule_settings($rule);

  //help
  //rules_admin_element_help($render['event'], rules_get_events('all', $element['#event']));
  $form['elements'] = array(
    '#type' => 'fieldset',
    '#title' => t('Rule elements'),
    '#collapsible' => TRUE,
  );

  //this will render the whole rule with the help of drupal_render(),
  //see rules.admin_render.inc
  $form['elements']['rule'] = $proxy->get_indexed_rule();
  $form['elements']['rule']['#theme'] = array('rules_admin_rule_render');

  return $form;
}

function rules_admin_form_edit_rule_submit($form, &$form_state) {
  $rule_ref = &$form_state['proxy']->get_rule();
  foreach (array('label', 'active', 'weight', 'set') as $key) {
    $rule_ref['#'. $key] = $form_state['values'][$key];
  }

  $form_state['proxy']->save_changes();
  rules_clear_cache();
  drupal_set_message(t("The rule %label has been updated.", array('%label' => $rule_ref['#label'])));
}


/**
 * Shows the revert configuration form
 */
function rules_admin_form_revert(&$form_state, $proxy) {
  $form_state['proxy'] = &$proxy;
  $rule = $proxy->get_rule();
  _rules_element_defaults($rule);

  $form = array();
  $path['path'] = isset($_GET['destination']) ? $_GET['destination'] : RULES_ADMIN_PATH;
  
  return confirm_form($form,
    t('Are you sure you want to revert the rule %label to its defaults?', array('%label' => $rule['#label'])),
    $path,
    t('This action cannot be undone.'), t('Revert'), t('Cancel')
  );
}

function rules_admin_form_revert_submit($form_id, $form_state) {
  $rule = $form_state['proxy']->get_rule();
  $form_state['proxy']->delete_rule();
  rules_clear_cache();
  drupal_set_message(t("The rule %label has been reverted to its defaults.", array('%label' => $rule['#label'])));
  $form_state['redirect'] = RULES_ADMIN_PATH;
}

/*
 * Returns the form for the add operation
 * This handles adding conditions and actions
 *
 * @param $configuration The name of the configuration, which is to be edited
 * @param $id Optionally specifies an element where the condition / action is to be added
 * @param $type Either 'action' or 'condition' or 'op'
 */
function rules_admin_form_add($proxy, $id, $type, $form_values = NULL) {
  $rule = $proxy->get_rule();
  _rules_element_defaults($rule);
  
  if (($cfg = rules_get_configurations('all', $configuration_name)) && in_array($type, array('action', 'condition', 'op'))) {
    $element = isset($form_values) ? array('#type' => $type, '#name' => $form_values['name']) : array('#type' => $type);
    $element += rules_get_element_info($element);

    //regenerate the ids
    rules_element_id($configuration_name, $cfg);
    //get the item assocaited with $id
    if ($parent = rules_element_id($configuration_name, $cfg, intval($id))) {
      $add = isset($form_values['added']) ? '' : '_add';
      if (function_exists($function = 'rules_admin_form_edit_'. $element['#type'] . $add)) {
        $form = array();
        $form['#multistep'] = TRUE;
        $form['cfg'] = array('#type' => 'value', '#value' => &$cfg);
        $form['added'] = array('#type' => 'hidden', '#value' => isset($form_values) ? 2 : 1);
        $form['element'] = array('#type' => 'value', '#value' => $element);
        $form['id'] = array('#type' => 'value', '#value' => $id);
        $form['submit'] = array('#type' => 'submit', '#weight' => 10, '#value' => $add ? t('Add') : t('Submit'));
        return $function($form, $element, $parent);
      }
    }
  }
  drupal_not_found();
}

/*
 * Returns the form for the first action add page
 */
function rules_admin_form_edit_action_add($form, $element) {
  $argument_info = rules_admin_event_get_argument_info($form['cfg']['#value']);
  $actions = rules_admin_filter_elements($argument_info, rules_get_actions());
  $form['name'] = array(
    '#type' => 'select',
    '#title' => t('Select an action to add'),
    '#options' => rules_get_grouped_labels($actions),
    '#required' => TRUE,
  );
  $form['#redirect'] = FALSE;
  return $form;
}

/*
 * Returns the form for the first condition add page
 */
function rules_admin_form_edit_condition_add(&$form, $element, &$parent) {
  $parent += rules_get_element_info($parent);
  if ($parent['#type'] == 'configuration' || isset($parent['#logical_op'])) {
    $argument_info = rules_admin_event_get_argument_info($form['cfg']['#value']);
    $conditions = rules_admin_filter_elements($argument_info, rules_get_conditions());
    $form['name'] = array(
      '#type' => 'select',
      '#title' => t('Select the condition to add'),
      '#options' => rules_get_grouped_labels($conditions),
      '#required' => TRUE,
    );
    $form['#redirect'] = FALSE;
    return $form;
  }
}


/*
 * Indenting a condition
 * Adds a logical operation and place the given condition element inside. We automatically
 * determine which operation is to be added.
 */
function rules_admin_form_edit_op_add(&$form, $element, &$parent) {
  $rule_name = $form['cfg']['#value']['#name'];
  $cfg = &rules_element_id($rule_name, $form['cfg']['#value'], 1);
  //get the condition
  $element = &rules_element_id($rule_name, $cfg, intval($form['id']['#value']));

  //determine the appropriate operation and apply it
  $op = rules_determine_operation($rule_name, $cfg, intval($form['id']['#value']));
  if (isset($element['#weight'])) {
    $weight = $element['#weight'];
    unset($element['#weight']);
  }
  $element = rules_configure($op, $element);

  if (isset($weight)) {
    //apply the weight of the element to the op
    $element['#weight'] = $weight;
  }

  //and save
  rules_admin_save_rule($cfg);
  rules_clear_cache();
  drupal_goto(RULES_ADMIN_PATH);
}

/*
 * Submit handler for the add element form
 */
function rules_admin_form_add_submit($form_id, $form_values) {
  if ($form_values['added'] == 2) {
    //get the reference on the currently edited configuration!
    $cfg = &rules_element_id($form_values['cfg']['#name'], $form_values['cfg'], 1);
    //get the reference on the associated element with $id
    $parent = &rules_element_id($cfg['#name'], $cfg, intval($form_values['id']));
    $element = $form_values['element'];

    //apply the changes to the element
    if (function_exists($function = 'rules_admin_form_edit_'. $element['#type'] . '_submit')) {
      $function($form_values, $element);
    }
    //don't save default values
    foreach ($form_values['element'] as $key => $value) {
      if (!in_array($key, array('#name', '#type')) && $value == $element[$key]) {
        unset($element[$key]);
      }
    }
    //remove empty values
    $element = array_filter($element);
    //just add the element to the parent
    $parent = rules_configure($parent, $element);

    rules_admin_save_rule($cfg);
    rules_clear_cache();
  }
}

/*
 * Returns the form for the edit operation
 * This handles editing conditions and actions
 *
 * @param $configuration The name of the configuration on which we operate
 * @param $id Specifies an element, which is to be edited.
 */
function rules_admin_form_edit($configuration_name, $id) {
  if ($cfg = rules_get_configurations('all', $configuration_name)) {
    if ($_POST['op'] == t('Delete')) {
      if ($_REQUEST['destination']) {
        $destination = drupal_get_destination();
        unset($_REQUEST['destination']);
      }
      drupal_goto(RULES_ADMIN_PATH .'/'. $cfg['#name'] .'/delete/'. $id, $destination);
    }
    //regenerate the ids
    rules_element_id($configuration_name, $cfg);
    //get the item assocaited with $id
    $element = rules_element_id($configuration_name, $cfg, intval($id));
    $element += rules_get_element_info($element);
    $type = isset($element['#logical_op']) && $element['#logical_op'] ? 'op' : $element['#type'];

    //call the appropriate function for each element type
    if (isset($element) && function_exists($function = 'rules_admin_form_edit_'. $type)) {
      $form = array();
      $form['cfg'] = array('#type' => 'value', '#value' => &$cfg);
      $form['element'] = array('#type' => 'value', '#value' => $element);
      $form['element_type'] = array('#type' => 'value', '#value' => $type);
      $form['id'] = array('#type' => 'value', '#value' => $id);
      $form['submit'] = array('#type' => 'submit', '#weight' => 10, '#value' => t('Submit'));
      $form['delete'] = array('#type' => 'submit', '#weight' => 11, '#value' => t('Delete'));
      return $function($form, $element);
    }
  }
  drupal_not_found();
}

function rules_admin_form_edit_submit($form_id, $form_values) {
  //get the reference on the currently edited configuration!
  $cfg = &rules_element_id($form_values['cfg']['#name'], $form_values['cfg'], 1);
  //get the reference on the associated element with $id
  $element = &rules_element_id($cfg['#name'], $cfg, intval($form_values['id']));
  $orig_element = rules_element_id($cfg['#name'], $cfg, intval($form_values['id']));

  //apply the changes to the element
  if (function_exists($function = 'rules_admin_form_edit_'. $form_values['element_type'] . '_submit')) {
    $function($form_values, $element);
  }
  //remove empty values
  $element = array_filter($element);

  //we only save if there were really changes to the element.
  //so a default configuration may keep its default status
  if ($element != $orig_element) {
    rules_admin_save_rule($cfg);
    rules_clear_cache();
  }
}

/*
 * Returns the edit form for an action
 */
function rules_admin_form_edit_action($form, $element) {
  $form['label'] = array(
    '#title' => t('Label'),
    '#type' => 'textfield',
    '#description' => t('Choose an appropriate label for this action.'),
    '#default_value' => $element['#label'],
    '#required' => TRUE,
  );
  if ($element['#label']) {
    drupal_set_title(t('Editing action %label', array('%label' => $element['#label'])));
  }
  rules_admin_element_help($form, $element);
  rules_admin_argument_map_form($form, $element);
  rules_admin_new_arguments_form($form, $element);
  rules_admin_element_configuration_form($form, $element);
  $form['weight'] = array(
    '#title' => t('Weight'),
    '#type' => 'weight',
    '#description' => t('Adjust the weight to customize the ordering of actions.'),
    '#default_value' => $element['#weight'] > 499 ? $element['#weight'] - 500 : $element['#weight'],
  );
  return $form;
}

/*
 * Apply the changes to the element
 */
function rules_admin_form_edit_action_submit($form_values, &$element) {
  foreach (array('label', 'weight') as $key) {
    $element['#'. $key] = $form_values[$key];
  }
  //save the argument map
  if (isset($form_values['map'])) {
    $element['#argument map'] = array_flip($form_values['map']);
  }
  rules_admin_new_arguments_form_submit($form_values, $element);
  rules_admin_element_configuration_form_submit($form_values, $element);
  drupal_set_message(t('The action %label has been saved.', array('%label' => $element['#label'])));
}

/*
 * Returns the edit form for a condition
 */
function rules_admin_form_edit_condition($form, $element) {
  $form['label'] = array(
    '#title' => t('Label'),
    '#type' => 'textfield',
    '#description' => t('Choose an appropriate label for this condition.'),
    '#default_value' => $element['#label'],
    '#required' => TRUE,
  );
  if ($element['#label']) {
    drupal_set_title(t('Editing condition %label', array('%label' => $element['#label'])));
  }
  $form['negate'] = array(
    '#title' => t('Negate'),
    '#type' => 'checkbox',
    '#description' => t('If checked, the condition returns TRUE, if it evaluates to FALSE.'),
    '#default_value' => $element['#negate'],
  );
  rules_admin_element_help($form, $element);
  rules_admin_argument_map_form($form, $element);
  rules_admin_element_configuration_form($form, $element);
  $form['weight'] = array(
    '#title' => t('Weight'),
    '#type' => 'weight',
    '#description' => t('Adjust the weight to customize the ordering of conditions.'),
    '#default_value' => isset($element['#weight']) ? $element['#weight'] : 0,
  );
  return $form;
}

/*
 * Apply the changes to the element
 */
function rules_admin_form_edit_condition_submit($form_values, &$element) {
  foreach (array('label', 'negate', 'weight') as $key) {
    $element['#'. $key] = $form_values[$key];
  }
  //save the argument map
  if (isset($form_values['map'])) {
    $element['#argument map'] = array_flip($form_values['map']);
  }
  rules_admin_element_configuration_form_submit($form_values, $element);
  drupal_set_message(t('The condition %label has been saved.', array('%label' => $element['#label'])));
}

/*
 * Returns the edit form of a logical operation
 */
function rules_admin_form_edit_op($form, $element) {
  drupal_set_title(t('Editing condition group %label', array('%label' => $element['#label'])));
  $form['negate'] = array(
    '#title' => t('Negate'),
    '#type' => 'checkbox',
    '#description' => t('If checked, the operation will be negated. E.g. AND would be handled as NOT AND.'),
    '#default_value' => $element['#negate'],
  );
  $form['operation'] = array(
    '#title' => t('Operation'),
    '#type' => 'select',
    '#description' => t('The logical operation of this condition group. E.g. if you select AND, this condition group will only evaluate to TRUE if all conditions of this group evaluate to TRUE.'),
    '#default_value' => $element['#type'],
    '#options' => rules_elements_get_logical_ops(),
    '#required' => TRUE,
  );
  $form['weight'] = array(
    '#title' => t('Weight'),
    '#type' => 'weight',
    '#description' => t('Adjust the weight to customize the ordering.'),
    '#default_value' => isset($element['#weight']) ? $element['#weight'] : 0,
  );
  return $form;
}

/*
 * Apply the changes to the element
 */
function rules_admin_form_edit_op_submit($form_values, &$element) {
  foreach (array('negate', 'weight') as $key) {
    $element['#'. $key] = $form_values[$key];
  }
  //remove empty values
  $element = array_filter($element);
  $element['#type'] = $form_values['operation'];
  drupal_set_message(t('The condition group %label has been saved.', array('%label' => $element['#type'])));
}

/*
 * Returns the configuration form of an element
 */
function rules_admin_element_configuration_form(&$form, $element) {
  rules_include_file(strtolower($element['#module']) .'_rules_forms');
  $function = $element['#name']. '_form';

  if (function_exists($function)) {
    $event = rules_get_events('all', $form['cfg']['#value']['#event']);
    _rules_element_defaults($event);
    //TODO: for d6 remove argument_info
    $argument_info = rules_admin_event_get_argument_info($form['cfg']['#value'], $element); 
    $info = array('element' => &$element, 'event' => &$event);

    $form['configure'] = $function($element['#settings'], $argument_info, $info);
    $form['#validate'][$element['#name']. '_validate'] = array();
  }
}

function rules_admin_element_configuration_form_submit($form_values, &$element) {
  $function = $element['#name']. '_submit';
  if (function_exists($function)) {
    $element['#settings'] =  $function($form_values['form_id'], $form_values);
  }
}

/*
 * Returns form elements for new arguments
 */
function rules_admin_new_arguments_form(&$form, $element) {
  if (isset($element['#new arguments']) && count($element['#new arguments'])) {
    $form['new_arguments'] = array(
      '#type' => 'fieldset',
      '#title' => t('New argument settings'),
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
      '#tree' => TRUE,
    );
    foreach ($element['#new arguments'] as $name => $argument) {
      $form['new_arguments'][$name] = array(
        '#type' => 'fieldset',
        '#title' => check_plain($argument['#label']),
      );
      $form['new_arguments'][$name]['label'] = array(
        '#type' => 'textfield',
        '#title' => t('Label'),
        '#default_value' => $argument['#label'],
        '#required' => TRUE,
      );
      $form['new_arguments'][$name]['entity'] = array(
        '#type' => 'value',
        '#value' => $argument['#entity']
      );
      $form['new_arguments'][$name]['default_name'] = array(
        '#type' => 'value',
        '#value' => isset($argument['#default_name']) ? $argument['#default_name'] : $name,
      );
      $form['new_arguments'][$name]['name'] = array(
        '#type' => 'textfield',
        '#title' => t('Machine readable argument name'),
        '#description' => t('Specify a unique name containing only digits, numbers and underscores.'),
        '#default_value' => $name,
        '#required' => TRUE,
      );
    }
    $form['#validate']['rules_admin_new_arguments_form_validate'] = array();
  }
}

function rules_admin_new_arguments_form_validate($form_id, $form_values, $form) {
  $argument_info = rules_admin_event_get_argument_info($form_values['cfg'], $form_values['element']);
  foreach ($form_values['new_arguments'] as $name => $argument) {
    if (isset($argument_info[$argument['name']])) {
      form_set_error(implode('][', array('new_arguments', $name, 'name')), t('This name already exists.'));
    }
    if (!eregi('^[0-9a-z_]*$', $argument['name'])) {
      form_set_error(implode('][', array('new_arguments', $name, 'name')), t('The name contains not allowed characters.'));
    }
  }
}

function rules_admin_new_arguments_form_submit($form_values, &$element) {
  if (isset($form_values['new_arguments'])) {
    unset($element['#new arguments']);
    foreach ($form_values['new_arguments'] as $name => $argument) {
      $element['#new arguments'][$argument['name']] = array(
        '#entity' => $argument['entity'],
        '#label' => $argument['label'],
        '#default_name' => $argument['default_name'],
      );
    }
  }
}

/*
 * Returns the argument mapping form
 */
function rules_admin_argument_map_form(&$form, $element) {
  $function = $element['#name']. '_form';
  if (is_array($element['#arguments']) && count($element['#arguments'])) {
    $form['map'] = array(
      '#type' => 'fieldset',
      '#title' => t('Arguments configuration'),
      '#tree' => TRUE,
    );
    $argument_info = rules_admin_event_get_argument_info($form['cfg']['#value'], $element);

    foreach ($element['#arguments'] as $name => $info) {
      
      $args = rules_admin_get_possible_arguments($info, $argument_info);
      $form['map'][$name] = array(
        '#type' => 'select',
        '#title' => $info['#label'],
        '#options' => $args,
        '#default_value' => rules_admin_map_get_configured_argument($name, $element),
      );
    }
  }
}

/*
 * Gets the possible arguments (= of the same entity) for an argument
 */
function rules_admin_get_possible_arguments($info, $argument_info, $print_error = TRUE) {
  $matches = array();
  foreach ($argument_info as $name => $this_info) {
    if (rules_is_valid_entity($this_info['#entity'], $info)) {
      $matches[$name] = $this_info['#label'];
    }
  }
  if ($print_error && count($matches) == 0) {
    drupal_set_message(t('There are not all necessary arguments available.'), 'error');
    drupal_goto(RULES_ADMIN_PATH);
  }
  return $matches;
}

/*
 * Gets the in the argument map configured argument for the given argument - or the default value
 */
function rules_admin_map_get_configured_argument($name, $element) {
  if (!isset($element['#argument map'])) {
    //per default try the the same name
    return $name;
  }
  $reverse_map = array_flip($element['#argument map']);
  return $reverse_map[$name];
}

/*
 * Shows the element description
 */
function rules_admin_element_help(&$form, $element) {
  if (isset($element['#description'])) {
    $form['help'] = array(
      '#type' => 'fieldset',
      '#title' => t('Description'),
      '#description' => $element['#description'],
    );
  }
}

/**
 * Shows the delete form for rules, conditions and actions
 */
function rules_admin_form_delete(&$form_state, $proxy, $element = NULL) {

  $form_state['proxy'] = &$proxy;
  $rule = $proxy->get_rule();
  _rules_element_defaults($rule);

  $form = array();
  $path = array();
  $path['path'] = isset($_GET['destination']) ? $_GET['destination'] : RULES_ADMIN_PATH;

  if (!isset($element)) {
    return confirm_form($form,
      t('Are you sure you want to delete the rule %label?', array('%label' => $rule['#label'])),
      $path,
      t('This action cannot be undone.'), t('Delete'), t('Cancel')
    );
  }
  else {
    //get the item assocaited with $id
    $form_state['id'] = $element['#id'];
    if ($info = rules_get_element_info($element)) {
      if (isset($info['#logical_op']) && $info['#logical_op']) {
        $form_state['is_op'] = TRUE;
        $text = t('Are you sure you want to delete the logical operation %label?', array('%label' => rules_get_element_label($element)));
      }
      else {
        $text = t('Are you sure you want to delete %label?', array('%label' => rules_get_element_label($element)));
      }
      return confirm_form($form, $text, $path, t('This action cannot be undone.'), t('Delete'), t('Cancel'));
    }
    drupal_not_found();
    exit;
  }
}

function rules_admin_form_delete_submit($form_id, $form_state) {
  $rule = $form_state['proxy']->get_rule();
  if (!isset($form_state['id'])) {
    $form_state['proxy']->delete_rule();
    rules_clear_cache();
    drupal_set_message(t("The rule %label has been deleted.", array('%label' => $rule['#label'])));
  }
  else {
    //get a reference on the element
    $element = &$form_state['proxy']->get_element(intval($form_state['id']));

    if (isset($form_state['is_op']) && $form_state['is_op']) {
      drupal_set_message(t("The logical operation %label has been deleted.", array('%label' => rules_get_element_label($element))));
      //just unset the type, so that containing conditions are not deleted
      unset($element['#type']);
    }
    else {
      drupal_set_message(t("%label has been deleted.", array('%label' => rules_get_element_label($element))));
      $element = NULL;
      //remove the elements key by cleaning the rule
    }
    $form_state['proxy']->clean_rule();
    $form_state['proxy']->save_changes();
    rules_clear_cache();
  }
  $form_state['redirect'] = RULES_ADMIN_PATH;
}

/**
 * Clones a rule
 */
function rules_admin_form_clone(&$form_state, $proxy) {

  $form_state['proxy'] = &$proxy;
  $rule = $proxy->get_rule();
  _rules_element_defaults($rule);
  
  drupal_set_message(t('Alter the settings for the cloned rule.'));

  $form_state['set'] = strpos($rule['#set'], 'event_') !== 0;
  $form = rules_admin_form_rule_settings($rule, $form_state['set']);
  $form['#submit'] = array('rules_admin_form_configuration_add_submit');
  return $form;
}

/**
 * Gets all compatible sets, which means that all arguments are available in the other set too
 */
function rules_admin_get_compatible_sets($rule, $sets) {
  if (isset($rule['#set']) && $rule['#set']) {
    $set = rules_get_rule_sets($rule['#set']);
    foreach ($sets as $key => $this_set) {
      if (array_diff(array_keys($set['arguments']), array_keys($this_set['arguments']))) {
        unset($sets[$key]);
      }
    }
  }
  return $sets;
}





/**
 * Gets an array of element labels grouped by modules
 *
 * @param $elements An array of info elements, e.g. as returned from rules_get_events()
 */
function rules_get_grouped_labels($elements) {
  $grouped = array();
  $ungrouped = array();

  foreach ($elements as $name => $element) {
    if (!isset($element['fixed']) || !$element['fixed']) {
      if (isset($element['module'])) {
        $grouped[$element['module']][$name] = $element['label'];
      }
      else {
        $ungrouped[$name] = $element['label'];
      }
    }
  }
  array_map('asort', $grouped);
  ksort($grouped);
  asort($ungrouped);
  return $ungrouped + $grouped;
}


/*
 * Determines which operation should be added
 * If the parent operation is an AND, we add an OR, and vice versa.
 */
function rules_determine_operation($rule_name, $cfg, $id) {

  $parent_id = rules_get_element_parent_id($rule_name, $cfg, $id);
  $parent = rules_element_id($rule_name, $elements, $parent_id);

  switch ($parent['#type']) {
    default:
    case 'configuration':
    case 'AND':
      return 'OR';
    case 'OR':
      return 'AND';
  }
}

/*
 * Returns a list of available logical operations suitable for use with #options
 */
function rules_elements_get_logical_ops() {
  $elements = rules_gather_data('elements');
  $labels = array_map('rules_get_element_label', array_filter($elements, '_rules_element_is_logical_op'));
  asort($labels);
  return $labels;
}

function _rules_element_is_logical_op($element) {
  return isset($element['#logical_op']) && $element['#logical_op'];
}

/*
 * Imports a configuration
 */
function rules_admin_form_import() {
  $form = array();
  $form['import'] = array(
    '#type' => 'textarea',
    '#title' => t('Rule configurations to import'),
    '#description' => t('Paste exported rules here to import them.'),
    '#rows' => 15,
    '#required' => TRUE,
  );
  $form['button'] = array('#type' => 'submit', '#weight' => 10, '#value' => t('Import'));
  return $form;
}

function rules_admin_form_import_submit($form_id, $form_values) {
  @eval('$cfgs = '. $form_values['import'] .';');
  if (isset($cfgs) && is_array($cfgs) && count($cfgs)) {
    foreach ($cfgs as $name => $data) {
      if (strtolower($data['#module']) == 'rules-ng') {
        //generate a new unique name for this configuration
        $data['#name'] = 'cfg_'. db_next_id('{rules_cfg}_name');
      }
      rules_admin_save_rule($data);
      drupal_set_message(t('Imported rule configuration %label.', array('%label' => rules_get_element_label($data))));
    }
    rules_clear_cache();
  }
  else {
    drupal_set_message(t('Import failed.'), 'error');
  }
}


/*
 * Exports one ore more configurations
 */
function rules_admin_form_export($form_values = NULL) {
  $form = array();
  if (!isset($form_values)) {
    $form['cfg'] = array(
      '#type' => 'select',
      '#title' => t('Select the rule configurations to export'),
      '#options' => rules_get_grouped_labels(rules_get_configurations()),
      '#multiple' => TRUE,
    );
    $form['button'] = array('#type' => 'submit', '#weight' => 10, '#value' => t('Export'));
    $form['#multistep'] = TRUE;
    $form['#redirect'] = FALSE;
  }
  else {
    //show a textarea containg the exported configs
    $cfgs = array();
    foreach ($form_values['cfg'] as $key => $name) {
      $cfgs[$name] = rules_get_configurations('all', $name);
    }
    $form['result'] = array(
      '#type' => 'textarea',
      '#title' => t('Exported rule configurations'),
      '#description' => t('Copy these data and paste them into the import page, to import.'),
      '#rows' => 15,
      '#attributes' => array('readonly' => 'readonly'),
      '#default_value' => var_export($cfgs, TRUE),
    );
  }
  return $form;
}

/*
 * Filters the elements (actions, conditions), so that only elements
 * compatible with the event are kept, which means entities for all arguments
 * are available, so that the action/condition can be configured
 */
function rules_admin_filter_elements($argument_info, $elements) {
  $filtered = array();
  foreach ($elements as $key => $element) {
    if (is_array($element['#arguments']) && count($element['#arguments'])) {
      foreach ($element['#arguments'] as $name => $info) {
        if (count(rules_admin_get_possible_arguments($info, $argument_info, FALSE)) == 0) {
          continue 2; //don't add this element to the filtered one
        }
      }
    }
    $filtered[$key] = $element;
  }
  return $filtered;
}

/**
 * Returns an array of all existing categories
 */
function rules_admin_get_categories() {
  $categories = array();
  foreach (rules_get_configured_items('rules') as $rule) {
    if (isset($rule['#categories']) && is_array($rule['#categories']) && (!isset($rule['#fixed']) || !$rule['#fixed'])) {
      foreach ($rule['#categories'] as $category) {
        if (!in_array($category, $categories)) {
          $categories[$element['#categories']] = $element['#categories'];
        }
      }
    }
  }
  return $categories;
}

/*
 * Gets $argument_info, which contains all available arguments
 * For this we need to get the arguments of the event as well as further arguments that
 * might be provided by actions of this or preceding configurations
 * 
 * @param $cfg The configuration for which to get the argument info
 * @param $element An optional element, which is currently edited
 */
function rules_admin_event_get_argument_info($cfg, $element = NULL) {
  static $cache = array();

  if (isset($cache[$cfg['#name']])) {
    if (!isset($element) || !isset($element['#new arguments'])) {
      return $cache[$cfg['#name']];
    }
    //remove elements that are provided by the currently edited element
    $custom_arguments = $cache[$cfg['#name']];
    foreach (array_intersect(array_keys($custom_arguments), array_keys($element['#new arguments'])) as $name) {
      unset($custom_arguments[$name]);
    }
    return $custom_arguments;
  }

  $event = rules_get_events('all', $cfg['#event']);
  _rules_element_defaults($event);
  $argument_info = $event['#arguments'];

  //get the arguments provided by an action before
  $configurations = rules_event_get_configurations($cfg['#event']);
  $argument_info += rules_get_new_arguments($configurations, $cfg['#name']);

  $cache[$cfg['#name']] = $argument_info;
  return rules_admin_event_get_argument_info($cfg, $element);
}

/*
 * Gets all new arguments defined by actions, which are executed until the given
 * configuration 
 */
function rules_get_new_arguments(&$elements, &$rule_name) {
  if (!isset($rule_name)) {
    //if the name is NULL, we are behind the specified configuration
    return array();
  }

  //sort the elements
  $elements += rules_get_element_info($elements);
  uasort($elements, "_rules_element_sort");

  //add the new arguments, if any
  $arguments = isset($elements['#new arguments']) ? $elements['#new arguments'] : array();

  //recurse
  foreach (element_children($elements) as $key) {
    $arguments += rules_get_new_arguments($elements[$key], $rule_name);
  }

  if ($elements['#type'] == 'configuration' && $elements['#name'] == $rule_name) {
    //element found, stop adding new arguments
    $rule_name = NULL;
    return $arguments;
  }
  return $arguments;
}
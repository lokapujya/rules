<?php

/**
 * @file
 * Contains \Drupal\rules\Engine\RulesConditionInterface.
 */

namespace Drupal\rules\Engine;

use Drupal\Core\Condition\ConditionInterface;
use Drupal\rules\Context\ContextAwarePluginInterface;

/**
 * Extends the core ConditionInterface to provide a negate() method.
 */
interface RulesConditionInterface extends ConditionInterface, ContextAwarePluginInterface {

  /**
   * Negates the result after evaluating this condition.
   *
   * @param bool $negate
   *   TRUE to indicate that the conditon should be negated, FALSE otherwise.
   *
   * @return $this
   *   The current condition object for chaining.
   */
  public function negate($negate = TRUE);

}

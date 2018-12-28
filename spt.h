/**
 * Spanning Tree contruction using Multi-Shout protocol.
 *
 * The protocol uses a neighbor network that were previously constructed is
 * some way (e.g. using the Discovery protocol).
 *
 * Protocol execution may be started in two ways:
 * 1. Spontaneously at a random instant.
 * 2. When a contruction message is received while in idle state.
 *
 * Only nodes that start the protocol spontaneusly are candidates for the
 * root (leader) election.
 *
 * This strategy allows the generation of less construction messages and
 * random node election.
 *
 * The candidate with smaller UID will be elected as the SPT root (leader).
 *
 *
 * Detailed Algorithm description
 *
 * Every initiator start the construction of its own uniquely identified
 * spanning-tree, but then suppressing some of these constructions, allowing
 * only one to complete. In this approach, an entity faced with two different
 * spt-constructions, will select and act on only one, “killing” the other;
 * the entity continues this selection process as long as it receives
 * conflicting requests.
 *
 * The criterion commonly used is based on min-id: since each spt-construction
 * has a unique id (that of its initiator), when faced with different
 * spt-constructions an entity will choose the one with the smallest id,
 * and terminate all the others.
 *
 * An entity U, at any time, participates in the construction of just one
 * spanning tree rooted in some initiator X. It will ignore all messages
 * referring to the construction of other spanning trees where the initiators
 * have larger ids than X. If instead U receives a message referring to the
 * construction of a spanning-tree rooted in an initiator Y with an id smaller
 * than X’s, then U will stop working for X and start working for Y.
 *
 * It is possible that an entity has already terminated its part of the
 * construction of a spanning tree when it receives a message from another
 * initiator (possibly, with a smaller id). In other words, when an entity
 * has terminated a construction, it does not know whether it might have
 * to restart again. Thus, it is necessary to include in the protocol a
 * mechanism that ensures an effective local termination for each entity.
 *
 * Local termination can be achieved by ensuring that we use, as a building
 * block, a unique-initiator spt-protocol in which the initiator will know
 * when its spanning tree has been completely constructed.
 */

#ifndef SPT_H_
#define SPT_H_

#include "misc.h"
#include "tpl.h"

/**
 * SPT protocol states. @{
 */
/** Protocol not started. */
#define SPT_STATE_IDLE      0
/** Protocol is running. */
#define SPT_STATE_ACTIVE    1
/** Protocol is terminating. */
#define SPT_STATE_TERM      2
/** Protocol is finished. */
#define SPT_STATE_DONE      3
/** @} */

/** Maximum number of tree node children */
#define SPT_CHILD_MAX       32

/**
 * Spanning tree protocol context.
 */
struct spt_ctx {
    uint32_t    start;      /**< Used for start spontaneous event. */
    uint8_t     state;      /**< Context state. */
    addr_t      root;       /**< Current root. */
    uint8_t     counter;    /**< Neighbor Q messages counter. */
    uint8_t     checks;     /**< Children CHECK messages counter. */
    addr_t      parent;     /**< SPT parent. */
    addr_t      child[SPT_CHILD_MAX]; /**< SPT children. */
    uint8_t     nchild;     /**< SPT number of children. */
    uint8_t     notify_num; /**< Number of pending children messages (ASK/TERM) */
    addr_t      notify_skp; /**< Child to skip during notification */
};

/** Spanning tree protocol context type alias. */
typedef struct spt_ctx spt_ctx_t;

/** Spanning tree protocol initialization. */
void spt_init(void);

/** Spanning tree protocol loop. */
void spt_loop(void);

#endif /* SPT_H_ */

/* SPDX-License-Identifier: BSD-2-Clause */
/*******************************************************************************
 * Copyright 2017-2018, Fraunhofer SIT sponsored by Infineon Technologies AG
 * All rights reserved.
 ******************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "tss2_mu.h"
#include "tss2_sys.h"
#include "tss2_esys.h"

#include "esys_types.h"
#include "esys_iutil.h"
#include "esys_mu.h"
#define LOGMODULE esys
#include "util/log.h"
#include "util/aux_util.h"

/** One-Call function for TPM2_ObjectChangeAuth
 *
 * This function invokes the TPM2_ObjectChangeAuth command in a one-call
 * variant. This means the function will block until the TPM response is
 * available. All input parameters are const. The memory for non-simple output
 * parameters is allocated by the function implementation.
 *
 * @param[in,out] esysContext The ESYS_CONTEXT.
 * @param[in]  objectHandle Handle of the object.
 * @param[in]  parentHandle Handle of the parent.
 * @param[in]  shandle1 Session handle for authorization of objectHandle
 * @param[in]  shandle2 Second session handle.
 * @param[in]  shandle3 Third session handle.
 * @param[in]  newAuth New authorization value.
 * @param[out] outPrivate Private area containing the new authorization value.
 *             (callee-allocated)
 * @retval TSS2_RC_SUCCESS if the function call was a success.
 * @retval TSS2_ESYS_RC_BAD_REFERENCE if the esysContext or required input
 *         pointers or required output handle references are NULL.
 * @retval TSS2_ESYS_RC_BAD_CONTEXT: if esysContext corruption is detected.
 * @retval TSS2_ESYS_RC_MEMORY: if the ESAPI cannot allocate enough memory for
 *         internal operations or return parameters.
 * @retval TSS2_ESYS_RC_BAD_SEQUENCE: if the context has an asynchronous
 *         operation already pending.
 * @retval TSS2_ESYS_RC_INSUFFICIENT_RESPONSE: if the TPM's response does not
 *          at least contain the tag, response length, and response code.
 * @retval TSS2_ESYS_RC_MALFORMED_RESPONSE: if the TPM's response is corrupted.
 * @retval TSS2_ESYS_RC_RSP_AUTH_FAILED: if the response HMAC from the TPM
           did not verify.
 * @retval TSS2_ESYS_RC_MULTIPLE_DECRYPT_SESSIONS: if more than one session has
 *         the 'decrypt' attribute bit set.
 * @retval TSS2_ESYS_RC_MULTIPLE_ENCRYPT_SESSIONS: if more than one session has
 *         the 'encrypt' attribute bit set.
 * @retval TSS2_ESYS_RC_BAD_TR: if any of the ESYS_TR objects are unknown
 *         to the ESYS_CONTEXT or are of the wrong type or if required
 *         ESYS_TR objects are ESYS_TR_NONE.
 * @retval TSS2_RCs produced by lower layers of the software stack may be
 *         returned to the caller unaltered unless handled internally.
 */
TSS2_RC
Esys_ObjectChangeAuth(
    ESYS_CONTEXT *esysContext,
    ESYS_TR objectHandle,
    ESYS_TR parentHandle,
    ESYS_TR shandle1,
    ESYS_TR shandle2,
    ESYS_TR shandle3,
    const TPM2B_AUTH *newAuth,
    TPM2B_PRIVATE **outPrivate)
{
    TSS2_RC r;

    r = Esys_ObjectChangeAuth_Async(esysContext, objectHandle, parentHandle,
                                    shandle1, shandle2, shandle3, newAuth);
    return_if_error(r, "Error in async function");

    /* Set the timeout to indefinite for now, since we want _Finish to block */
    int32_t timeouttmp = esysContext->timeout;
    esysContext->timeout = -1;
    /*
     * Now we call the finish function, until return code is not equal to
     * from TSS2_BASE_RC_TRY_AGAIN.
     * Note that the finish function may return TSS2_RC_TRY_AGAIN, even if we
     * have set the timeout to -1. This occurs for example if the TPM requests
     * a retransmission of the command via TPM2_RC_YIELDED.
     */
    do {
        r = Esys_ObjectChangeAuth_Finish(esysContext, outPrivate);
        /* This is just debug information about the reattempt to finish the
           command */
        if (base_rc(r) == TSS2_BASE_RC_TRY_AGAIN)
            LOG_DEBUG("A layer below returned TRY_AGAIN: %" PRIx32
                      " => resubmitting command", r);
    } while (base_rc(r) == TSS2_BASE_RC_TRY_AGAIN);

    /* Restore the timeout value to the original value */
    esysContext->timeout = timeouttmp;
    return_if_error(r, "Esys Finish");

    return TSS2_RC_SUCCESS;
}

/** Asynchronous function for TPM2_ObjectChangeAuth
 *
 * This function invokes the TPM2_ObjectChangeAuth command in a asynchronous
 * variant. This means the function will return as soon as the command has been
 * sent downwards the stack to the TPM. All input parameters are const.
 * In order to retrieve the TPM's response call Esys_ObjectChangeAuth_Finish.
 *
 * @param[in,out] esysContext The ESYS_CONTEXT.
 * @param[in]  objectHandle Handle of the object.
 * @param[in]  parentHandle Handle of the parent.
 * @param[in]  shandle1 Session handle for authorization of objectHandle
 * @param[in]  shandle2 Second session handle.
 * @param[in]  shandle3 Third session handle.
 * @param[in]  newAuth New authorization value.
 * @retval ESYS_RC_SUCCESS if the function call was a success.
 * @retval TSS2_ESYS_RC_BAD_REFERENCE if the esysContext or required input
 *         pointers or required output handle references are NULL.
 * @retval TSS2_ESYS_RC_BAD_CONTEXT: if esysContext corruption is detected.
 * @retval TSS2_ESYS_RC_MEMORY: if the ESAPI cannot allocate enough memory for
 *         internal operations or return parameters.
 * @retval TSS2_RCs produced by lower layers of the software stack may be
           returned to the caller unaltered unless handled internally.
 * @retval TSS2_ESYS_RC_MULTIPLE_DECRYPT_SESSIONS: if more than one session has
 *         the 'decrypt' attribute bit set.
 * @retval TSS2_ESYS_RC_MULTIPLE_ENCRYPT_SESSIONS: if more than one session has
 *         the 'encrypt' attribute bit set.
 * @retval TSS2_ESYS_RC_BAD_TR: if any of the ESYS_TR objects are unknown
 *         to the ESYS_CONTEXT or are of the wrong type or if required
 *         ESYS_TR objects are ESYS_TR_NONE.
 */
TSS2_RC
Esys_ObjectChangeAuth_Async(
    ESYS_CONTEXT *esysContext,
    ESYS_TR objectHandle,
    ESYS_TR parentHandle,
    ESYS_TR shandle1,
    ESYS_TR shandle2,
    ESYS_TR shandle3,
    const TPM2B_AUTH *newAuth)
{
    TSS2_RC r;
    LOG_TRACE("context=%p, objectHandle=%"PRIx32 ", parentHandle=%"PRIx32 ","
              "newAuth=%p",
              esysContext, objectHandle, parentHandle, newAuth);
    TSS2L_SYS_AUTH_COMMAND auths;
    RSRC_NODE_T *objectHandleNode;
    RSRC_NODE_T *parentHandleNode;
    TPM2B_AUTH authCopy =
        { .size = 0,
          .buffer = {}
        };
    TPMI_ALG_HASH hashAlg = 0;

    /* Check context, sequence correctness and set state to error for now */
    if (esysContext == NULL) {
        LOG_ERROR("esyscontext is NULL.");
        return TSS2_ESYS_RC_BAD_REFERENCE;
    }
    r = iesys_check_sequence_async(esysContext);
    if (r != TSS2_RC_SUCCESS)
        return r;
    esysContext->state = _ESYS_STATE_INTERNALERROR;

    /* Check input parameters */
    r = check_session_feasibility(shandle1, shandle2, shandle3, 1);
    return_state_if_error(r, _ESYS_STATE_INIT, "Check session usage");

    /* Retrieve the metadata objects for provided handles */
    r = esys_GetResourceObject(esysContext, objectHandle, &objectHandleNode);
    return_state_if_error(r, _ESYS_STATE_INIT, "objectHandle unknown.");
    r = esys_GetResourceObject(esysContext, parentHandle, &parentHandleNode);
    return_state_if_error(r, _ESYS_STATE_INIT, "parentHandle unknown.");

    if (objectHandleNode->rsrc.rsrcType == IESYSC_KEY_RSRC) {
        hashAlg = objectHandleNode->rsrc.misc.rsrc_key_pub.publicArea.nameAlg;
    }

    if (newAuth) {
        authCopy = *newAuth;
    };

    r = iesys_adapt_auth_value(&esysContext->crypto_backend, &authCopy, hashAlg);
    return_state_if_error(r, _ESYS_STATE_INIT, "Adapt auth value");

    /* Initial invocation of SAPI to prepare the command buffer with parameters */
    r = Tss2_Sys_ObjectChangeAuth_Prepare(esysContext->sys,
                                          (objectHandleNode == NULL)
                                           ? TPM2_RH_NULL
                                           : objectHandleNode->rsrc.handle,
                                          (parentHandleNode == NULL)
                                           ? TPM2_RH_NULL
                                           : parentHandleNode->rsrc.handle,
                                          &authCopy);
    return_state_if_error(r, _ESYS_STATE_INIT, "SAPI Prepare returned error.");

    /* Calculate the cpHash Values */
    r = init_session_tab(esysContext, shandle1, shandle2, shandle3);
    return_state_if_error(r, _ESYS_STATE_INIT, "Initialize session resources");
    if (objectHandleNode != NULL)
        iesys_compute_session_value(esysContext->session_tab[0],
                &objectHandleNode->rsrc.name, &objectHandleNode->auth);
    else
        iesys_compute_session_value(esysContext->session_tab[0], NULL, NULL);

    iesys_compute_session_value(esysContext->session_tab[1], NULL, NULL);
    iesys_compute_session_value(esysContext->session_tab[2], NULL, NULL);

    /* Generate the auth values and set them in the SAPI command buffer */
    r = iesys_gen_auths(esysContext, objectHandleNode, parentHandleNode, NULL, &auths);
    return_state_if_error(r, _ESYS_STATE_INIT,
                          "Error in computation of auth values");

    esysContext->authsCount = auths.count;
    if (auths.count > 0) {
        r = Tss2_Sys_SetCmdAuths(esysContext->sys, &auths);
        return_state_if_error(r, _ESYS_STATE_INIT, "SAPI error on SetCmdAuths");
    }

    /* Trigger execution and finish the async invocation */
    r = Tss2_Sys_ExecuteAsync(esysContext->sys);
    return_state_if_error(r, _ESYS_STATE_INTERNALERROR,
                          "Finish (Execute Async)");

    esysContext->state = _ESYS_STATE_SENT;

    return r;
}

/** Asynchronous finish function for TPM2_ObjectChangeAuth
 *
 * This function returns the results of a TPM2_ObjectChangeAuth command
 * invoked via Esys_ObjectChangeAuth_Finish. All non-simple output parameters
 * are allocated by the function's implementation. NULL can be passed for every
 * output parameter if the value is not required.
 *
 * @param[in,out] esysContext The ESYS_CONTEXT.
 * @param[out] outPrivate Private area containing the new authorization value.
 *             (callee-allocated)
 * @retval TSS2_RC_SUCCESS on success
 * @retval ESYS_RC_SUCCESS if the function call was a success.
 * @retval TSS2_ESYS_RC_BAD_REFERENCE if the esysContext or required input
 *         pointers or required output handle references are NULL.
 * @retval TSS2_ESYS_RC_BAD_CONTEXT: if esysContext corruption is detected.
 * @retval TSS2_ESYS_RC_MEMORY: if the ESAPI cannot allocate enough memory for
 *         internal operations or return parameters.
 * @retval TSS2_ESYS_RC_BAD_SEQUENCE: if the context has an asynchronous
 *         operation already pending.
 * @retval TSS2_ESYS_RC_TRY_AGAIN: if the timeout counter expires before the
 *         TPM response is received.
 * @retval TSS2_ESYS_RC_INSUFFICIENT_RESPONSE: if the TPM's response does not
 *         at least contain the tag, response length, and response code.
 * @retval TSS2_ESYS_RC_RSP_AUTH_FAILED: if the response HMAC from the TPM did
 *         not verify.
 * @retval TSS2_ESYS_RC_MALFORMED_RESPONSE: if the TPM's response is corrupted.
 * @retval TSS2_RCs produced by lower layers of the software stack may be
 *         returned to the caller unaltered unless handled internally.
 */
TSS2_RC
Esys_ObjectChangeAuth_Finish(
    ESYS_CONTEXT *esysContext,
    TPM2B_PRIVATE **outPrivate)
{
    TSS2_RC r;
    LOG_TRACE("context=%p, outPrivate=%p",
              esysContext, outPrivate);

    if (esysContext == NULL) {
        LOG_ERROR("esyscontext is NULL.");
        return TSS2_ESYS_RC_BAD_REFERENCE;
    }

    /* Check for correct sequence and set sequence to irregular for now */
    if (esysContext->state != _ESYS_STATE_SENT &&
        esysContext->state != _ESYS_STATE_RESUBMISSION) {
        LOG_ERROR("Esys called in bad sequence.");
        return TSS2_ESYS_RC_BAD_SEQUENCE;
    }
    esysContext->state = _ESYS_STATE_INTERNALERROR;

    /* Allocate memory for response parameters */
    if (outPrivate != NULL) {
        *outPrivate = calloc(sizeof(TPM2B_PRIVATE), 1);
        if (*outPrivate == NULL) {
            return_error(TSS2_ESYS_RC_MEMORY, "Out of memory");
        }
    }

    /*Receive the TPM response and handle resubmissions if necessary. */
    r = Tss2_Sys_ExecuteFinish(esysContext->sys, esysContext->timeout);
    if (base_rc(r) == TSS2_BASE_RC_TRY_AGAIN) {
        LOG_DEBUG("A layer below returned TRY_AGAIN: %" PRIx32, r);
        esysContext->state = _ESYS_STATE_SENT;
        goto error_cleanup;
    }
    /* This block handle the resubmission of TPM commands given a certain set of
     * TPM response codes. */
    if (r == TPM2_RC_RETRY || r == TPM2_RC_TESTING || r == TPM2_RC_YIELDED) {
        LOG_DEBUG("TPM returned RETRY, TESTING or YIELDED, which triggers a "
            "resubmission: %" PRIx32, r);
        if (esysContext->submissionCount++ >= _ESYS_MAX_SUBMISSIONS) {
            LOG_WARNING("Maximum number of (re)submissions has been reached.");
            esysContext->state = _ESYS_STATE_INIT;
            goto error_cleanup;
        }
        esysContext->state = _ESYS_STATE_RESUBMISSION;
        r = Tss2_Sys_ExecuteAsync(esysContext->sys);
        if (r != TSS2_RC_SUCCESS) {
            LOG_WARNING("Error attempting to resubmit");
            /* We do not set esysContext->state here but inherit the most recent
             * state of the _async function. */
            goto error_cleanup;
        }
        r = TSS2_ESYS_RC_TRY_AGAIN;
        LOG_DEBUG("Resubmission initiated and returning RC_TRY_AGAIN.");
        goto error_cleanup;
    }
    /* The following is the "regular error" handling. */
    if (iesys_tpm_error(r)) {
        LOG_WARNING("Received TPM Error");
        esysContext->state = _ESYS_STATE_INIT;
        goto error_cleanup;
    } else if (r != TSS2_RC_SUCCESS) {
        LOG_ERROR("Received a non-TPM Error");
        esysContext->state = _ESYS_STATE_INTERNALERROR;
        goto error_cleanup;
    }

    /*
     * Now the verification of the response (hmac check) and if necessary the
     * parameter decryption have to be done.
     */
    r = iesys_check_response(esysContext);
    goto_state_if_error(r, _ESYS_STATE_INTERNALERROR, "Error: check response",
                        error_cleanup);

    /*
     * After the verification of the response we call the complete function
     * to deliver the result.
     */
    r = Tss2_Sys_ObjectChangeAuth_Complete(esysContext->sys,
                                           (outPrivate != NULL) ? *outPrivate
                                            : NULL);
    goto_state_if_error(r, _ESYS_STATE_INTERNALERROR,
                        "Received error from SAPI unmarshaling" ,
                        error_cleanup);

    esysContext->state = _ESYS_STATE_INIT;

    return TSS2_RC_SUCCESS;

error_cleanup:
    if (outPrivate != NULL)
        SAFE_FREE(*outPrivate);

    return r;
}
